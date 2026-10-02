[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Archive,
    [Parameter(Mandatory = $true)]
    [ValidateSet('linux-x64', 'macos-x64', 'macos-arm64')][string]$Platform,
    [switch]$SkipSmoke,
    [switch]$ExpectLive2D
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$testRunner = Join-Path $PSScriptRoot 'test-unix.sh'
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("BongoCatPackage_" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try {
    Push-Location $temporaryRoot
    try {
        cmake -E tar xf $archivePath
        if ($LASTEXITCODE -ne 0) { throw 'Package extraction failed' }
        $roots = @(Get-ChildItem -LiteralPath $temporaryRoot -Directory)
        if ($roots.Count -ne 1 -or $roots[0].Name -notmatch "^BongoCat(-X)?-[0-9].*-$Platform$") {
            throw 'Expected one production BongoCat package directory'
        }
        $root = $roots[0].FullName
        # No build ships the Core binary - users supply it at runtime.
        $leaked = @(Get-ChildItem -LiteralPath $root -Recurse -File |
            Where-Object { $_.Name -match '^Live2DCubismCore\.(dll|so|dylib)$' })
        if ($leaked.Count) {
            throw "Core binary leaked into package: $($leaked.FullName -join ', ')"
        }
        if ($Platform.StartsWith('macos-')) {
            $executable = Join-Path $root 'BongoCat.app/Contents/MacOS/BongoCat'
            $assets = Join-Path $root 'BongoCat.app/Contents/Resources/assets'
        } else {
            $executable = Join-Path $root 'BongoCat'
            $assets = Join-Path $root 'assets'
        }
        # Build shape is passed in explicitly: builds with Live2D support
        # must embed the Framework shaders, diagnostic builds must not.
        $hasLive2D = $ExpectLive2D.IsPresent
        $required = @($executable) + @(
            'bongocat.png', 'locales/en-US.json',
            'models/standard/cat.model3.json',
            'models/standard/demomodel.moc3',
            'models/standard/demomodel.1024/texture_00.png'
        )
        if ($hasLive2D) {
            $required += 'FrameworkShaders/VertShaderSrc.vert',
                'FrameworkShaders/FragShaderSrc.frag',
                'FrameworkShaders/VertShaderSrcBlend.vert',
                'FrameworkShaders/FragShaderSrcBlend.frag'
        }
        $required = $required | ForEach-Object {
            if ([IO.Path]::IsPathRooted($_)) { $_ } else { Join-Path $assets $_ }
        }
        foreach ($path in $required) {
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
                (Get-Item -LiteralPath $path).Length -eq 0) {
                throw "Package resource missing or empty: $path"
            }
        }
        $licenses = @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter LICENSE)
        if ($licenses.Count) { throw "Unexpected loose LICENSE files: $($licenses.FullName -join ', ')" }
        Write-Host "Package layout verified: $Platform"
        if (-not $hasLive2D) {
            Write-Host "Diagnostic package: skipping the Cubism smoke test"
        }
        if (-not $SkipSmoke -and $hasLive2D) {
            $storage = Join-Path $temporaryRoot 'smoke-data'
            if ($Platform -eq 'linux-x64') {
                # Runtime-Core packages ship without the Core binary; drop
                # it in the same way an end user would (data-dir live2d
                # folder) so the smoke test exercises the real runtime
                # import path.
                $coreLib = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot `
                    '../../vendor/CubismSdkForNative/Core/dll/linux/x86_64/libLive2DCubismCore.so'))
                if (-not (Test-Path -LiteralPath $coreLib -PathType Leaf)) {
                    throw 'Cubism Core library not found in the restored SDK'
                }
                $live2dDir = Join-Path $storage 'data/live2d'
                New-Item -ItemType Directory -Path $live2dDir -Force |
                    Out-Null
                Copy-Item -LiteralPath $coreLib -Destination $live2dDir
            }
            $appArgs = @($executable, '--ci-smoke', '--ci-ignore-global-input',
                '--ci-live2d-scenario=visual-consistency',
                "--storage-root=$storage")
            # BONGOCAT_SMOKE_GDB=1 runs the packaged app under gdb so a
            # crash in the smoke test prints a full backtrace.
            if ($env:BONGOCAT_SMOKE_GDB -and $Platform -eq 'linux-x64') {
                & bash $testRunner env BONGO_CAT_DISABLE_NEARBY_MODEL_SCAN=1 `
                    gdb -batch -return-child-result `
                    -ex 'handle SIGPIPE nostop noprint pass' `
                    -ex run -ex 'thread apply all bt full' --args @appArgs
            } else {
                & bash $testRunner env BONGO_CAT_DISABLE_NEARBY_MODEL_SCAN=1 `
                    @appArgs
            }
            $smokeExitCode = $LASTEXITCODE
            if ($smokeExitCode -ne 0) {
                Get-ChildItem -LiteralPath $storage -Recurse -File -Filter 'live2d-*audit.*' |
                    ForEach-Object {
                        Write-Host "Live2d audit: $($_.FullName)"
                        Get-Content -LiteralPath $_.FullName | Write-Host
                    }
                Get-ChildItem -LiteralPath $storage -Recurse -File -Filter 'runtime-diagnostics.log' |
                    ForEach-Object {
                        Write-Host "Runtime diagnostics: $($_.FullName)"
                        Get-Content -LiteralPath $_.FullName -ErrorAction SilentlyContinue |
                            Select-Object -Last 60 | Write-Host
                    }
                throw "Packaged application smoke test failed (exit code $smokeExitCode)"
            }
            $audits = @(Get-ChildItem -LiteralPath $storage -Recurse -File -Filter live2d-audit.txt)
            if ($audits.Count -ne 1) { throw 'Packaged application produced no unique Live2D audit' }
            $audit = Get-Content -LiteralPath $audits[0].FullName -Raw
            if ($audit -notmatch '(?m)^renderer=cubism-native\r?$' -or
                $audit -notmatch '(?m)^assertions=passed\r?$') {
                throw "Packaged Live2D verification failed: $audit"
            }
            Write-Host "Packaged native Live2D smoke test passed: $Platform"
        }
    } finally {
        Pop-Location
    }
} finally {
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
}
