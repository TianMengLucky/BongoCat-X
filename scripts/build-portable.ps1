<#
.SYNOPSIS
Builds the Windows portable release archive (ZIP) for BongoCat.

.DESCRIPTION
Configures and builds a Release tree through scripts/build-windows.ps1, then
runs the CMake "package-portable" target. CPack packs the executable, the
renderer plugins, the licenses and the model assets into
<BuildDir>\dist\<product>-<version>-<platform>-portable.zip and writes a
matching .sha256 checksum next to it. After the archive is built the script
also copies it (and its checksum) to the current user's Desktop.

Unlike "build-windows.ps1 -Package" this needs neither Inno Setup nor the
Windows 10/11 SDK: the portable archive is the only artifact produced.

.PARAMETER Configuration
Release (default) or RelWithDebInfo. Debug archives are not supported.

.PARAMETER BuildDir
Build tree to use; defaults to <repo>\build-cubism so the FetchContent
dependencies are shared with the debug tree. Relative paths are resolved
against the repository root.

.PARAMETER RequireCubism
Fail configuration when the Live2D Cubism SDK is missing, and ship the
bongo_live2d plugin in the archive. Without it the package contains the
diagnostic backend notice instead of the plugin.

.PARAMETER SkipConfigure
Reuse the existing CMake cache instead of reconfiguring.

.PARAMETER Clean
Delete the build tree before building.

.PARAMETER NoChecksum
Do not write the .sha256 checksum file next to the archive.

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-portable.ps1

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-portable.ps1 `
    -BuildDir build-release -RequireCubism
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = '',
    [switch]$RequireCubism,
    [switch]$SkipConfigure,
    [switch]$Clean,
    [switch]$NoChecksum
)

$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)

$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
if (-not $BuildDir) { $BuildDir = Join-Path $root 'build-cubism' }
if (-not [IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $root $BuildDir
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

# The Visual Studio installer ships a complete CMake distribution under
# Common7\IDE\CommonExtensions; it is the only CMake on machines that have
# the IDE but no standalone install. build-windows.ps1 applies the same
# fallback while configuring, so mirror it for the packaging step.
function Resolve-CMakeExecutable {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
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

# --- 1. Release build (shared with the normal Windows build) -------------
# Invoked in this process so the environment hygiene in build-windows.ps1
# (duplicate proxy variables, PATH canonicalisation) also covers the
# packaging commands below.
$buildScript = Join-Path $PSScriptRoot 'build-windows.ps1'
if (-not (Test-Path -LiteralPath $buildScript)) {
    Write-Host "Build script not found: $buildScript"
    exit 1
}
# Named splatting: an array would be bound as positional values here and
# '-Configuration' itself would trip the ValidateSet inside the build script.
$buildParameters = @{ Configuration = $Configuration; BuildDir = $BuildDir }
if ($RequireCubism) { $buildParameters.RequireCubism = $true }
if ($SkipConfigure) { $buildParameters.SkipConfigure = $true }
if ($Clean) { $buildParameters.Clean = $true }
try {
    & $buildScript @buildParameters
} catch {
    Write-Host "Release build failed: $($_.Exception.Message)"
    exit 1
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "Release build failed (exit code $LASTEXITCODE)."
    exit $LASTEXITCODE
}

# --- 2. Portable archive -------------------------------------------------
# package-portable drives CPack with the Runtime install component, which
# already carries the executable, the plugins, the licenses and the model
# assets. Building bongo_cat also builds and stages the renderer plugins.
$cmake = Resolve-CMakeExecutable
if (-not $cmake) {
    Write-Host 'CMake was not found. Install CMake or add the Visual Studio'
    Write-Host '"Desktop development with C++" workload, then retry.'
    exit 1
}
Write-Host ''
Write-Host 'Building the portable archive...'
& $cmake --build $BuildDir --config $Configuration --target package-portable
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Portable packaging failed.'
    Write-Host "Packaging build directory: $BuildDir"
    exit $LASTEXITCODE
}

$nameFile = Join-Path $BuildDir 'bongocat-package-name.txt'
if (-not (Test-Path -LiteralPath $nameFile)) {
    Write-Host "The package name file was not produced: $nameFile"
    exit 1
}
$packageName = (Get-Content -LiteralPath $nameFile -Raw).Trim()
$archive = Join-Path $BuildDir "dist/$packageName-portable.zip"
if (-not (Test-Path -LiteralPath $archive)) {
    Write-Host "Portable archive is missing: $archive"
    exit 1
}

$archiveSize = [Math]::Round((Get-Item -LiteralPath $archive).Length / 1MB, 2)
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if (-not $NoChecksum) {
    "$hash  $([IO.Path]::GetFileName($archive))" |
        Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
}

Write-Host ''
Write-Host 'Portable archive contents:'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $entries = @($zip.Entries | Where-Object { -not [string]::IsNullOrEmpty($_.Name) })
    Write-Host ("  {0} files" -f $entries.Count)
    foreach ($entry in @($entries | Sort-Object FullName | Select-Object -First 10)) {
        Write-Host ('  ' + $entry.FullName)
    }
    if ($entries.Count -gt 10) {
        Write-Host ("  ... and {0} more" -f ($entries.Count - 10))
    }
} finally {
    $zip.Dispose()
}

# Copy the portable archive and its checksum to the current user's Desktop
# so the finished package is easy to find. The build-tree copy is kept too.
$desktop = [Environment]::GetFolderPath('Desktop')
$desktopArchive = $null
if (-not [string]::IsNullOrWhiteSpace($desktop) -and (Test-Path -LiteralPath $desktop)) {
    Copy-Item -LiteralPath $archive -Destination $desktop -Force
    if (Test-Path -LiteralPath "$archive.sha256") {
        Copy-Item -LiteralPath "$archive.sha256" -Destination $desktop -Force
    }
    $desktopArchive = Join-Path $desktop ([IO.Path]::GetFileName($archive))
} else {
    Write-Host 'Desktop folder unavailable; kept the archive in the build tree only.'
}

Write-Host ''
Write-Host "Portable package: $archive"
if ($desktopArchive) { Write-Host "Copied to Desktop: $desktopArchive" }
Write-Host ("Size: {0} MB" -f $archiveSize)
Write-Host "SHA256: $hash"
exit 0