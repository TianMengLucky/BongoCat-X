# Publishing guard policy for GitHub workflows.
#
# 1. Any job that publishes artifacts, scans them, or touches store
#    packaging must be gated to a known upstream repository.
# 2. The Live2D Cubism SDK is proprietary and must never be restored or
#    built from CI. Only mac-app-store.yml is allowed to reference it;
#    SDK-enabled binaries are built locally by users (see README).
[CmdletBinding()]
param([switch]$SelfTest)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$upstreamGuards = @(
    "github.repository == 'vladelaina/BongoCat'",
    "github.repository == 'TianMengLucky/BongoCat-X'"
)
$workflowDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'workflows'
$sensitiveMarkers = @(
    'build-store-package.ps1',
    'CUBISM_SDK_ARCHIVE_URL',
    'VIRUSTOTAL_API_KEY',
    'actions/upload-artifact',
    'softprops/action-gh-release'
)
$cubismSdkMarkers = @(
    'restore-cubism-sdk',
    '-DBONGO_CAT_REQUIRE_CUBISM=ON',
    '-RequireCubism',
    'CUBISM_SDK_ARCHIVE_URL',
    'vendor/CubismSdkForNative'
)
$sdkAllowedWorkflow = 'mac-app-store.yml'

function Get-PublishingGuardFailures {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Label,
        [bool]$AllowCubismSdk = $false
    )

    $failures = @()
    if (-not $AllowCubismSdk) {
        $sdkMatches = @($cubismSdkMarkers | Where-Object {
            $Text.Contains($_)
        })
        if ($sdkMatches.Count -gt 0) {
            $failures += "$Label`: restores or builds the proprietary " +
                "Cubism SDK; matched $($sdkMatches -join ', ')"
        }
    }

    $jobsStart = $Text.IndexOf("jobs:", [StringComparison]::Ordinal)
    if ($jobsStart -lt 0) { return $failures }
    $jobsText = $Text.Substring($jobsStart)
    $jobPattern = '(?ms)^  ([A-Za-z0-9_-]+):\s*\r?\n' +
        '(.*?)(?=^  [A-Za-z0-9_-]+:\s*\r?\n|\z)'
    $jobMatches = [regex]::Matches($jobsText, $jobPattern)

    foreach ($job in $jobMatches) {
        $jobName = $job.Groups[1].Value
        $jobText = $job.Value
        $matched = @($sensitiveMarkers | Where-Object { $jobText.Contains($_) })
        if ($matched.Count -eq 0) { continue }
        $guarded = @($upstreamGuards | Where-Object {
            $jobText.Contains($_)
        }).Count -gt 0
        if (-not $guarded) {
            $failures += "$Label`: job '$jobName' lacks upstream guard; " +
                "matched $($matched -join ', ')"
        }
    }
    return $failures
}

$workflowPaths = @(
    Get-ChildItem -LiteralPath $workflowDirectory -File |
        Where-Object { $_.Extension -in @('.yml', '.yaml') } |
        Sort-Object FullName
)
$failures = @()
$checkedJobs = 0
$selfTestText = $null

foreach ($path in $workflowPaths) {
    $text = Get-Content -LiteralPath $path.FullName -Raw
    $pathFailures = @(Get-PublishingGuardFailures -Text $text `
        -Label $path.Name -AllowCubismSdk:($path.Name -eq $sdkAllowedWorkflow))
    $failures += $pathFailures
    $jobsStart = $text.IndexOf("jobs:", [StringComparison]::Ordinal)
    if ($jobsStart -ge 0) {
        $jobsText = $text.Substring($jobsStart)
        $checkedJobs += @([regex]::Matches($jobsText,
            '(?ms)^  ([A-Za-z0-9_-]+):\s*\r?\n(.*?)(?=^  [A-Za-z0-9_-]+:\s*\r?\n|\z)') |
            Where-Object {
                $jobText = $_.Value
                @($sensitiveMarkers | Where-Object {
                    $jobText.Contains($_)
                }).Count -gt 0
            }).Count
    }
    if (-not $selfTestText -and
        ($sensitiveMarkers | Where-Object { $text.Contains($_) })) {
        $selfTestText = $text
    }
}

if ($SelfTest) {
    if (-not $selfTestText) {
        $failures += 'Self-test could not find a sensitive workflow.'
    }
    else {
        $mutated = $selfTestText
        foreach ($guard in $upstreamGuards) {
            $mutated = $mutated.Replace($guard, 'true')
        }
        $caught = @(Get-PublishingGuardFailures -Text $mutated -Label 'self-test')
        if ($caught.Count -eq 0) {
            $failures += 'Self-test did not reject a removed upstream guard.'
        }
        $withSdk = $selfTestText +
            "`n  - run: ./.github/scripts/restore-cubism-sdk.ps1`n"
        $sdkCaught = @(Get-PublishingGuardFailures -Text $withSdk -Label 'self-test')
        if (-not ($sdkCaught -match 'Cubism SDK')) {
            $failures += 'Self-test did not reject a Cubism SDK restore step.'
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Publishing guard policy failed:`n- " +
        ($failures -join "`n- "))
    exit 1
}

Write-Host "Publishing guard policy passed for $checkedJobs sensitive job(s)."
if ($SelfTest) { Write-Host 'Publishing guard negative self-tests passed.' }
