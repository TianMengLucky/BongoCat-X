[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [string]$OutputDirectory,
    [string]$CertificateOutputPath,
    [ValidateSet('x86', 'x64')]
    [string]$Architecture = 'x64',
    [switch]$SignForLocalTesting
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $scriptDirectory "..\..")).Path
$projectVersion = & (Join-Path $repositoryRoot 'packaging\get-project-version.ps1')
$Version = $projectVersion.AppVersion
$PackageVersion = $projectVersion.StorePackageVersion

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot "output\microsoft-store"
}

$ExecutablePath = (Resolve-Path $ExecutablePath).Path
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

$packageBaseName = if ($SignForLocalTesting) {
    "bongocat_${Version}_${Architecture}-local-test"
}
else {
    "bongocat_${Version}_${Architecture}"
}
$msixPath = Join-Path $OutputDirectory "$packageBaseName.msix"
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BongoCatStorePackage_" + [guid]::NewGuid())
$stagingDirectory = Join-Path $temporaryRoot "package"
$assetsDirectory = Join-Path $stagingDirectory "Assets"
$certificatePath = Join-Path $temporaryRoot "BongoCatStoreTemporary.pfx"
$certificate = $null

if ($CertificateOutputPath -and -not $SignForLocalTesting) {
    throw "CertificateOutputPath requires SignForLocalTesting. Store submission packages are unsigned."
}
if ($SignForLocalTesting -and -not $CertificateOutputPath) {
    $CertificateOutputPath = Join-Path $OutputDirectory "$packageBaseName.cer"
}

function Get-WindowsSdkTool {
    param([Parameter(Mandatory = $true)][string]$Name)

    $programFilesX86 = ${env:ProgramFiles(x86)}
    $sdkRoot = Join-Path $programFilesX86 "Windows Kits\10\bin"
    if (Test-Path $sdkRoot) {
        $sdkVersions = Get-ChildItem $sdkRoot -Directory |
            Where-Object { $_.Name -match '^\d+\.\d+' } |
            Sort-Object { [version]$_.Name } -Descending

        foreach ($sdkVersion in $sdkVersions) {
            $candidate = Join-Path $sdkVersion.FullName "x64\$Name"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $appCertKitCandidate = Join-Path $programFilesX86 "Windows Kits\10\App Certification Kit\$Name"
    if (Test-Path $appCertKitCandidate) {
        return $appCertKitCandidate
    }

    throw "$Name was not found. Install the Windows 10/11 SDK."
}

function New-StoreLogo {
    param(
        [Parameter(Mandatory = $true)][object]$Source,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $bitmap = [System.Drawing.Bitmap]::new(
        $Width,
        $Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

            $scale = [Math]::Min($Width / $Source.Width, $Height / $Source.Height)
            $drawWidth = [Math]::Max(1, [int][Math]::Round($Source.Width * $scale))
            $drawHeight = [Math]::Max(1, [int][Math]::Round($Source.Height * $scale))
            $drawX = [int](($Width - $drawWidth) / 2)
            $drawY = [int](($Height - $drawHeight) / 2)
            $graphics.DrawImage($Source, $drawX, $drawY, $drawWidth, $drawHeight)
        }
        finally {
            $graphics.Dispose()
        }

        $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory, $stagingDirectory, $assetsDirectory | Out-Null
    Remove-Item $msixPath -Force -ErrorAction SilentlyContinue

    Copy-Item $ExecutablePath (Join-Path $stagingDirectory "BongoCat.exe")
    $pluginDirectory = Join-Path (Split-Path -Parent $ExecutablePath) 'plugins'
    if (Test-Path -LiteralPath $pluginDirectory -PathType Container) {
        Copy-Item -LiteralPath $pluginDirectory -Destination (Join-Path $stagingDirectory 'plugins') -Recurse
    }

    $manifestTemplate = Get-Content (Join-Path $scriptDirectory "AppxManifest.xml.in") -Raw
    $manifest = $manifestTemplate.Replace("@PACKAGE_VERSION@", $PackageVersion)
    $manifest = $manifest.Replace("@PROCESSOR_ARCHITECTURE@", $Architecture)
    [System.IO.File]::WriteAllText(
        (Join-Path $stagingDirectory "AppxManifest.xml"),
        $manifest,
        [System.Text.UTF8Encoding]::new($false))

    Add-Type -AssemblyName System.Drawing
    $sourceLogo = [System.Drawing.Image]::FromFile((Join-Path $repositoryRoot "resources\assets\bongocat.png"))
    try {
        New-StoreLogo $sourceLogo 50 50 (Join-Path $assetsDirectory "StoreLogo.png")
        New-StoreLogo $sourceLogo 44 44 (Join-Path $assetsDirectory "Square44x44Logo.png")
        New-StoreLogo $sourceLogo 150 150 (Join-Path $assetsDirectory "Square150x150Logo.png")
    }
    finally {
        $sourceLogo.Dispose()
    }

    $makeAppx = Get-WindowsSdkTool "makeappx.exe"
    & $makeAppx pack /o /h SHA256 /d $stagingDirectory /p $msixPath
    if ($LASTEXITCODE -ne 0) {
        throw "MakeAppx failed with exit code $LASTEXITCODE."
    }

    if ($SignForLocalTesting) {
        $signTool = Get-WindowsSdkTool "signtool.exe"
        $publisher = "CN=5503A135-7FA4-466B-815C-DBE627F4065F"
        $certificate = New-SelfSignedCertificate `
            -Type Custom `
            -KeyUsage DigitalSignature `
            -KeyExportPolicy Exportable `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}") `
            -Subject $publisher `
            -FriendlyName "BongoCat temporary Microsoft Store package signing"

        $CertificateOutputPath = [System.IO.Path]::GetFullPath($CertificateOutputPath)
        New-Item -ItemType Directory -Force -Path (Split-Path $CertificateOutputPath -Parent) | Out-Null
        Export-Certificate -Cert $certificate -FilePath $CertificateOutputPath | Out-Null

        $plainPassword = [guid]::NewGuid().ToString("N")
        $securePassword = ConvertTo-SecureString $plainPassword -AsPlainText -Force
        Export-PfxCertificate -Cert $certificate -FilePath $certificatePath -Password $securePassword | Out-Null

        & $signTool sign /fd SHA256 /f $certificatePath /p $plainPassword $msixPath
        if ($LASTEXITCODE -ne 0) {
            throw "SignTool failed with exit code $LASTEXITCODE."
        }
    }

    if ($SignForLocalTesting) {
        Write-Host "Signed local test package created: $msixPath"
    }
    else {
        Write-Host "Unsigned Microsoft Store submission package created: $msixPath"
    }
    Write-Host "Package identity version: $PackageVersion"
    if ($SignForLocalTesting) {
        Write-Host "Certificate exported for local installation: $CertificateOutputPath"
    }
}
finally {
    if ($certificate) {
        Remove-Item ("Cert:\CurrentUser\My\" + $certificate.Thumbprint) -Force -ErrorAction SilentlyContinue
    }
    Remove-Item $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}
