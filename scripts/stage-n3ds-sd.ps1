[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DataWin,

    [string]$ThreeDsx = "build\n3ds\cinnamon.3dsx",

    [string]$OutputRoot = "build\sd-card",

    [string]$SdRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-PathFrom {
    param([string]$Path, [string]$Base)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $Base $Path))
}

$dataWinPath = Resolve-PathFrom -Path $DataWin -Base (Get-Location).Path
if (Test-Path -LiteralPath $dataWinPath -PathType Container) {
    $dataWinPath = Join-Path $dataWinPath "data.win"
}
if (-not (Test-Path -LiteralPath $dataWinPath -PathType Leaf)) {
    throw "data.win was not found: $dataWinPath"
}
$dataWinPath = (Resolve-Path -LiteralPath $dataWinPath).Path

$threeDsxPath = Resolve-PathFrom -Path $ThreeDsx -Base $repoRoot
if (-not (Test-Path -LiteralPath $threeDsxPath -PathType Leaf)) {
    throw ("cinnamon.3dsx was not found: " + $threeDsxPath + [Environment]::NewLine + "Run build-windows-n3ds.ps1 first.")
}
$threeDsxPath = (Resolve-Path -LiteralPath $threeDsxPath).Path

$stageRoot = Resolve-PathFrom -Path $OutputRoot -Base $repoRoot
$appDir = Join-Path $stageRoot "3ds\cinnamon"
New-Item -ItemType Directory -Force -Path $appDir | Out-Null

$staged3dsx = Join-Path $appDir "cinnamon.3dsx"
$stagedDataWin = Join-Path $appDir "data.win"

Copy-Item -LiteralPath $threeDsxPath -Destination $staged3dsx -Force
Copy-Item -LiteralPath $dataWinPath -Destination $stagedDataWin -Force

$threeDsxHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $staged3dsx).Hash.ToLowerInvariant()
$dataWinHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $stagedDataWin).Hash.ToLowerInvariant()

$buildManifestPath = Join-Path $repoRoot "build\repro\n3ds-build.json"
if (Test-Path -LiteralPath $buildManifestPath -PathType Leaf) {
    $buildManifest = Get-Content -LiteralPath $buildManifestPath -Raw | ConvertFrom-Json
    $expected3dsxHash = ([string]$buildManifest.cinnamon_3dsx_sha256).ToLowerInvariant()
    if ($expected3dsxHash -and $expected3dsxHash -ne $threeDsxHash) {
        throw "The staged cinnamon.3dsx does not match build\repro\n3ds-build.json. Expected $expected3dsxHash, got $threeDsxHash."
    }
    Write-Host "[OK] cinnamon.3dsx matches build\repro\n3ds-build.json."
}

$manifestDir = Join-Path $repoRoot "build\repro"
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null
$manifestPath = Join-Path $manifestDir "n3ds-sd-stage.json"

$manifest = [ordered]@{
    cinnamon_3dsx_sha256 = $threeDsxHash
    data_win_sha256 = $dataWinHash
    staged_app_dir = $appDir
    expected_sd_app_dir = "sdmc:/3ds/cinnamon"
    expected_dsp_firmware = "sdmc:/3ds/dspfirm.cdc"
    generated_utc = [DateTime]::UtcNow.ToString("o")
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

if ($SdRoot) {
    $sdRootPath = Resolve-PathFrom -Path $SdRoot -Base (Get-Location).Path
    if (-not (Test-Path -LiteralPath $sdRootPath -PathType Container)) {
        throw "SD root does not exist: $sdRootPath"
    }

    $sdAppDir = Join-Path $sdRootPath "3ds\cinnamon"
    New-Item -ItemType Directory -Force -Path $sdAppDir | Out-Null
    Copy-Item -LiteralPath $staged3dsx -Destination (Join-Path $sdAppDir "cinnamon.3dsx") -Force
    Copy-Item -LiteralPath $stagedDataWin -Destination (Join-Path $sdAppDir "data.win") -Force

    Write-Host ""
    Write-Host "Copied staged files to SD card:" -ForegroundColor Green
    Write-Host "  $sdAppDir"

    $dspFirm = Join-Path $sdRootPath "3ds\dspfirm.cdc"
    if (Test-Path -LiteralPath $dspFirm -PathType Leaf) {
        Write-Host "[OK] DSP firmware is present: $dspFirm"
    } else {
        Write-Warning "DSP firmware was not found at $dspFirm. Dump it on-console from Luma3DS Rosalina -> Miscellaneous options -> Dump DSP firmware before testing audio."
    }
}

Write-Host ""
Write-Host "New 3DS SD staging completed." -ForegroundColor Green
Write-Host "App dir:      $appDir"
Write-Host "3DSX SHA-256: $threeDsxHash"
Write-Host "data.win SHA: $dataWinHash"
Write-Host "Manifest:     $manifestPath"
Write-Host ""
Write-Host "Copy the staged '3ds\cinnamon' folder to the SD card's /3ds/ directory."
Write-Host "Also ensure /3ds/dspfirm.cdc exists on the SD card for NDSP audio."
