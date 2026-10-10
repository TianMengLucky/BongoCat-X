param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = '',
    [ValidateSet('x64', 'Win32')]
    [string]$Architecture = 'x64',
    [ValidateRange(1, 64)]
    [int]$Jobs = [Math]::Max(2, [Math]::Min(16, [Environment]::ProcessorCount)),
    [switch]$SkipConfigure,
    [switch]$SkipTests,
    [string[]]$Target = @('bongo_cat'),
    [switch]$RequireCubism,
    [switch]$Package,
    [switch]$Clean,
    # Reapply defaults to existing caches; -SkipConfigure reuses cached values.
    [bool]$OptimizeReleaseSize = $true,
    [bool]$OptimizeReleaseIpo = $true
)

$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$canonicalPath = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $canonicalPath
$env:VSLANG = '1033'
$env:MSBUILDDISABLENODEREUSE = '1'
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if (-not $BuildDir) { $BuildDir = Join-Path $root 'build-cubism' }
if (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $root $BuildDir
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$Target = @($Target | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($Target.Count -eq 0) {
    Write-Host 'At least one CMake target is required.'
    exit 1
}
if ($Clean -and $SkipConfigure) {
    Write-Host 'The -Clean and -SkipConfigure options cannot be used together.'
    exit 1
}
$esc = [char]27
$pink = '38;2;247;125;170'
$muted = '38;2;80;80;80'
$barWidth = 40

function Write-BuildProgress {
    param([int]$Percent, [string]$Message, [switch]$NewLine)
    $Percent = [Math]::Max(0, [Math]::Min(100, $Percent))
    $filled = [int][Math]::Floor($Percent * $script:barWidth / 100)
    $empty = $script:barWidth - $filled
    $fillText = '#' * $filled
    $emptyText = '.' * $empty
    $line = "`r${script:esc}[1m${script:esc}[$script:pink" +
        "m[$($Percent.ToString().PadLeft(3))%]${script:esc}[0m " +
        "${script:esc}[$script:pink" + "m$fillText" +
        "${script:esc}[$script:muted" + "m$emptyText${script:esc}[0m $Message"
    if ($NewLine) { Write-Host $line } else { Write-Host -NoNewline $line }
}

function Show-FailureLog {
    param([string[]]$Paths)
    Write-Host ''
    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path)) { continue }
        Get-Content -LiteralPath $path -Tail 30 | ForEach-Object { Write-Host $_ }
    }
}

function Write-GitHubBuildAnnotations {
    param([string]$Path)
    if ($env:GITHUB_ACTIONS -ne 'true' -or
        -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $pattern = 'FAILED:|fatal error|(?:warning|error) [A-Z]+\d+:|LNK\d+|MSB\d+: error|unresolved external|cannot open file|ninja: build stopped'
    Get-Content -LiteralPath $Path |
        Where-Object { $_ -match $pattern } |
        Select-Object -Last 30 |
        ForEach-Object {
            $message = $_.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
            Write-Output "::error title=Windows build failure::$message"
        }
}

# --- Environment hygiene -------------------------------------------------
# MSBuild builds a case-insensitive dictionary from the process environment
# and aborts when two names differ only in case:
#   error MSB6001: "CL.exe" ... System.ArgumentException: 已添加项
#   (key HTTP_PROXY, added key http_proxy)
# Windows itself tolerates that pair — its variable names are case-insensitive
# — but MSBuild does not, and the failure reaches us as a bogus
# "No CMAKE_C_COMPILER could be found" during configure. Proxy tooling and
# sandboxes commonly export both spellings, so collapse every duplicate to a
# single entry before configuring. Dropping one spelling is equivalent to
# keeping it: Windows matches the name case-insensitively.
function Merge-DuplicateEnvironmentNames {
    $guard = 0
    while ($guard -lt 32) {
        $guard++
        $names = @([System.Environment]::GetEnvironmentVariables('Process').Keys)
        $group = $names | Group-Object { $_.ToLowerInvariant() } |
            Where-Object { $_.Count -gt 1 } | Select-Object -First 1
        if (-not $group) { return }
        $victim = $group.Group[-1]
        [System.Environment]::SetEnvironmentVariable($victim, $null)
        Write-Host ("Merged environment variable that differed only in case: {0}" -f
            ($group.Group -join ' / '))
    }
}

Merge-DuplicateEnvironmentNames

# --- Toolchain discovery -------------------------------------------------
# The Visual Studio installer ships a complete CMake distribution under
# Common7\IDE\CommonExtensions; on machines that installed the IDE without a
# standalone CMake it is the only CMake present. It also always knows the
# generator name matching its own Visual Studio release.
function Get-VisualStudioCMakePath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
    foreach ($instance in @(& $vswhere -products * -property installationPath)) {
        if (-not $instance) { continue }
        $candidate = Join-Path $instance `
            'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

# Map a Visual Studio installation version to its CMake generator name.
function Get-VisualStudioGeneratorName {
    param([string]$InstallationVersion)
    if ($InstallationVersion -notmatch '^(\d+)\.') { return $null }
    switch ([int]$Matches[1]) {
        18 { return 'Visual Studio 18 2026' }
        17 { return 'Visual Studio 17 2022' }
        16 { return 'Visual Studio 16 2019' }
        15 { return 'Visual Studio 15 2017' }
        14 { return 'Visual Studio 14 2015' }
        default { return $null }
    }
}

function Test-CMakeGenerator {
    param([string]$Name)
    if (-not $Name) { return $false }
    return ((& cmake --help 2>&1 | Out-String) -match [regex]::Escape($Name))
}

# Pick the newest installed Visual Studio whose generator this CMake knows.
# Returns $null when none matches, which happens when the CMake on PATH
# predates the installed Visual Studio release (it then has no generator name
# for it); the caller retries with the Visual Studio bundled CMake.
function Resolve-VisualStudioGenerator {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
    $versions = @(& $vswhere -products * -format value `
        -property installationVersion) | Where-Object { $_ -match '^\d+\.' }
    $versions = $versions |
        Sort-Object -Descending { [int](($_ -split '\.')[0]) }
    foreach ($version in $versions) {
        $name = Get-VisualStudioGeneratorName $version
        if ($name -and (Test-CMakeGenerator $name)) { return $name }
    }
    return $null
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $bundledCmake = Get-VisualStudioCMakePath
    if ($bundledCmake) {
        $env:Path = "$(Split-Path -Parent $bundledCmake);$env:Path"
        Write-Host "CMake is not on PATH; using the Visual Studio bundled CMake: $bundledCmake"
    } else {
        Write-Host 'Error: CMake was not found in PATH and no Visual Studio bundled CMake was located.'
        Write-Host 'Install CMake (https://cmake.org/download/) or add the Visual Studio "Desktop development with C++" workload, then retry.'
        exit 1
    }
}

if ($Package -or $Target -contains 'package-installer') {
    try {
        $isccPath = & (Join-Path $root 'packaging/windows/find-inno.ps1')
        $env:Path = "$(Split-Path $isccPath -Parent);$env:Path"
        Write-Host "Inno Setup compiler: $isccPath"
    } catch {
        Write-Host $_.Exception.Message
        exit 1
    }
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    $rootPrefix = $root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $insideRoot = $BuildDir.StartsWith($rootPrefix,
        [StringComparison]::OrdinalIgnoreCase)
    if (-not $insideRoot -or (Split-Path $BuildDir -Leaf) -notlike 'build*') {
        Write-Host "Refusing to clean unexpected directory: $BuildDir"
        exit 1
    }
    Write-BuildProgress 2 'Cleaning previous build artifacts...'
    $depsPath = Join-Path $BuildDir '_deps'
    $preservedDeps = Join-Path (Split-Path $BuildDir -Parent) (
        '.bongocat-deps-' + [Guid]::NewGuid().ToString('N'))
    $hasDeps = Test-Path -LiteralPath $depsPath
    try {
        if ($hasDeps) {
            Move-Item -LiteralPath $depsPath -Destination $preservedDeps `
                -Force -ErrorAction Stop
        }
        Remove-Item -LiteralPath $BuildDir -Recurse -Force -ErrorAction Stop
        New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
        if ($hasDeps) {
            Move-Item -LiteralPath $preservedDeps `
                -Destination (Join-Path $BuildDir '_deps') -Force `
                -ErrorAction Stop
        }
    } catch {
        if ($hasDeps -and (Test-Path -LiteralPath $preservedDeps) -and
            -not (Test-Path -LiteralPath $depsPath)) {
            New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
            Move-Item -LiteralPath $preservedDeps -Destination $depsPath `
                -Force -ErrorAction SilentlyContinue
        }
        Write-Host "Cleaning failed: $($_.Exception.Message)"
        exit 1
    }
}

if ($SkipConfigure -and -not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt'))) {
    Write-Host "Cannot skip configuration because no CMake cache exists in: $BuildDir"
    exit 1
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$configureLog = Join-Path $BuildDir 'cmake_config.log'
$buildLog = Join-Path $BuildDir 'build.log'
$start = [DateTime]::UtcNow

Write-Host ''
Write-Host "BongoCat $Configuration build"
if ($SkipConfigure) {
    Write-BuildProgress 20 'Using existing CMake configuration.' -NewLine
} else {
    Write-BuildProgress 5 'Configuring project...'
    # Match the generator to the installed Visual Studio release: VS 18 (2026)
    # and VS 17 (2022) have distinct generator names, and a CMake older than
    # the IDE does not know the newer name. When the CMake on PATH cannot
    # drive any installed release, switch to the CMake bundled with Visual
    # Studio, which always knows its own generator.
    $generator = Resolve-VisualStudioGenerator
    if (-not $generator) {
        $bundledCmake = Get-VisualStudioCMakePath
        $activeCmake = (Get-Command cmake).Source
        if ($bundledCmake -and
            (Split-Path -Parent $bundledCmake) -ne
                (Split-Path -Parent $activeCmake)) {
            $env:Path = "$(Split-Path -Parent $bundledCmake);$env:Path"
            Write-Host "No generator matched the CMake on PATH; using $bundledCmake"
            $generator = Resolve-VisualStudioGenerator
        }
    }
    # BONGOCAT_GENERATOR overrides the auto-picked generator (e.g. "Ninja");
    # a Ninja configure requires cl.exe/link.exe already on PATH, so this is
    # meant for shells launched from the VS developer prompt.
    if ($env:BONGOCAT_GENERATOR) { $generator = $env:BONGOCAT_GENERATOR }
    if (-not $generator) {
        Write-Host 'Error: no CMake generator matches the installed Visual Studio.'
        Write-Host 'Upgrade CMake, or set BONGOCAT_GENERATOR to a generator this machine supports.'
        exit 1
    }
    $configureArgs = @(
        '-S', $root, '-B', $BuildDir,
        '-G', $generator,
        '-DBONGO_CAT_WARNINGS_AS_ERRORS=ON',
        "-DBONGO_CAT_OPTIMIZE_RELEASE_SIZE=$($OptimizeReleaseSize.ToString().ToUpperInvariant())",
        "-DBONGO_CAT_OPTIMIZE_RELEASE_IPO=$($OptimizeReleaseIpo.ToString().ToUpperInvariant())"
    )
    # -A is a Visual Studio generator concept; Ninja and NMake reject it.
    if ($generator -like 'Visual Studio *') {
        $configureArgs += @('-A', $Architecture)
    }
    # Always pass the value explicitly: release CI restores the licensed SDK
    # and opts in with -RequireCubism, while local diagnostic builds set it
    # off when the SDK is absent.
    if ($RequireCubism) {
        $configureArgs += '-DBONGO_CAT_REQUIRE_CUBISM=ON'
    } else {
        $configureArgs += '-DBONGO_CAT_REQUIRE_CUBISM=OFF'
    }
    if ($SkipTests) { $configureArgs += '-DBUILD_TESTING=OFF' }
    # Wrap the compilers with sccache when it is installed (CI does; local
    # machines without it keep the plain build).
    if (Get-Command sccache -ErrorAction SilentlyContinue) {
        $configureArgs += '-DCMAKE_C_COMPILER_LAUNCHER=sccache'
        $configureArgs += '-DCMAKE_CXX_COMPILER_LAUNCHER=sccache'
    }
    $configureWriter = New-Object IO.StreamWriter(
        $configureLog, $false, (New-Object Text.UTF8Encoding($false)))
    $configureActivity = 0
    $configurePercent = 5
    try {
        & cmake @configureArgs 2>&1 | ForEach-Object {
            $configureWriter.WriteLine($_.ToString())
            $configureActivity++
            $nextPercent = [Math]::Min(19,
                5 + [int][Math]::Floor([Math]::Sqrt($configureActivity)))
            if ($nextPercent -ne $configurePercent) {
                $configurePercent = $nextPercent
                Write-BuildProgress $configurePercent 'Configuring project...'
            }
        }
        $configureStatus = $LASTEXITCODE
    } finally {
        $configureWriter.Dispose()
    }
    if ($configureStatus -ne 0) {
        Write-BuildProgress 5 'Configuration failed.' -NewLine
        Write-GitHubBuildAnnotations $configureLog
        Show-FailureLog @($configureLog)
        Write-Host "Full log: $configureLog"
        exit 1
    }
    Write-BuildProgress 20 'Configuration complete.' -NewLine
}

Remove-Item -LiteralPath $buildLog -Force -ErrorAction SilentlyContinue
$projects = @(Get-ChildItem -LiteralPath $BuildDir -Recurse -Filter '*.vcxproj' `
    -ErrorAction SilentlyContinue)
$compileItems = 0
foreach ($project in $projects) {
    $compileItems += @(Select-String -LiteralPath $project.FullName `
        -SimpleMatch '<ClCompile Include=' -ErrorAction SilentlyContinue).Count
}
$compileItems = [Math]::Max(1, $compileItems)
$buildArgs = @('--build', $BuildDir, '--config', $Configuration,
    '--target') + $Target + @('--parallel', $Jobs)
$lastPercent = 20
$compiled = 0
$activity = 0
Write-BuildProgress $lastPercent ("Building target(s): {0}..." -f ($Target -join ', '))
$buildWriter = New-Object IO.StreamWriter(
    $buildLog, $false, (New-Object Text.UTF8Encoding($false)))
try {
    & cmake @buildArgs 2>&1 | ForEach-Object {
        $line = $_.ToString()
        $buildWriter.WriteLine($line)
        $activity++
        if ($line -match '\.(c|cc|cpp|cxx)(\s|$)') { $compiled++ }
        $compilePercent = 20 + [int][Math]::Floor(
            [Math]::Min(1.0, $compiled / [double]$compileItems) * 75)
        $activityPercent = 20 + [int][Math]::Min(72,
            [Math]::Floor([Math]::Sqrt($activity) * 4))
        $percent = [Math]::Min(95,
            [Math]::Max($lastPercent,
                [Math]::Max($compilePercent, $activityPercent)))
        if ($percent -ne $lastPercent) {
            $lastPercent = $percent
            $message = if ($compiled -gt 0) {
                "Compiling ($compiled files)..."
            } else { "Building target(s): $($Target -join ', ')..." }
            Write-BuildProgress $lastPercent $message
        }
    }
    $buildStatus = $LASTEXITCODE
} finally {
    $buildWriter.Dispose()
}

if ($buildStatus -ne 0) {
    Write-BuildProgress $lastPercent 'Build failed.' -NewLine
    Write-GitHubBuildAnnotations $buildLog
    Show-FailureLog @($buildLog)
    Write-Host "Full log: $buildLog"
    exit $buildStatus
}

if ($Target -contains 'bongo_cat') {
    $output = Join-Path (Join-Path $BuildDir $Configuration) 'BongoCat.exe'
    if (-not (Test-Path -LiteralPath $output)) {
        Write-BuildProgress 95 'BongoCat.exe was not produced.' -NewLine
        exit 1
    }
}
$elapsedTotal = [DateTime]::UtcNow - $start
Write-BuildProgress 100 'Build complete.' -NewLine
Write-Host ("Build time: {0:mm\:ss}" -f $elapsedTotal)
if ($output) { Write-Host "Output: $output" }
Write-Host "Logs: $buildLog"

if ($Package) {
    Write-Host ''
    Write-Host 'Building versioned portable, installer and MSIX packages...'
    $packageArgs = @('--build', $BuildDir, '--config', $Configuration,
        '--target', 'package-portable', 'package-installer', '--parallel', $Jobs)
    & cmake @packageArgs
    $packageStatus = $LASTEXITCODE
    if ($packageStatus -ne 0) {
        Write-Host 'Package generation failed.'
        Write-Host "Installer failure details: $(Join-Path $BuildDir 'installer.log')"
        Write-Host "Packaging build directory: $BuildDir"
        exit $packageStatus
    }
    $packageNameFile = Join-Path $BuildDir 'bongocat-package-name.txt'
    if (-not (Test-Path -LiteralPath $packageNameFile)) {
        Write-Host "Package name file was not produced: $packageNameFile"
        exit 1
    }
    $packageName = (Get-Content -LiteralPath $packageNameFile -Raw).Trim()
    $packageDist = Join-Path $BuildDir 'dist'
    # CPack's Windows generator is ZIP, so the portable artifact is an archive
    # (see cmake/Packaging.cmake); only the installer is an executable.
    $portable = Join-Path $packageDist "$packageName-portable.zip"
    $installer = Join-Path $packageDist "$packageName-setup.exe"
    if (-not (Test-Path -LiteralPath $portable) -or
        -not (Test-Path -LiteralPath $installer)) {
        Write-Host 'Package generation completed without both expected files.'
        Write-Host "Expected portable: $portable"
        Write-Host "Expected installer: $installer"
        exit 1
    }
    Write-Host "Portable package: $portable"
    Write-Host "Installer package: $installer"

    $msixNameFile = Join-Path $BuildDir 'bongocat-msix-name.txt'
    Remove-Item -LiteralPath $msixNameFile -Force -ErrorAction SilentlyContinue
    try {
        $storeArchitecture = if ($Architecture -eq 'Win32') { 'x86' } else { 'x64' }
        $projectVersion = & (Join-Path $root 'packaging/get-project-version.ps1')
        $msixName = "bongocat_$($projectVersion.AppVersion)_$storeArchitecture.msix"
        $msix = Join-Path $packageDist $msixName
        & (Join-Path $root 'packaging/microsoft-store/build-store-package.ps1') `
            -ExecutablePath (Join-Path $BuildDir "$Configuration/BongoCat.exe") `
            -Architecture $storeArchitecture -OutputDirectory $packageDist
        & (Join-Path $root 'packaging/microsoft-store/validate-store-package.ps1') `
            -PackagePath $msix -ExpectedArchitecture $storeArchitecture
        Set-Content -LiteralPath $msixNameFile -Value $msixName -Encoding ASCII `
            -ErrorAction Stop
    } catch {
        Write-Host "MSIX generation or validation failed: $($_.Exception.Message)"
        Write-Host 'MSIX packaging requires the Windows 10/11 SDK (makeappx.exe).'
        exit 1
    }
    Write-Host "Unsigned Microsoft Store package: $msix"
}
exit 0
