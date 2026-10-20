[CmdletBinding()]
param(
    [string]$BuildDir = "build\n3ds",
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$AssetDir = "build\game-assets\current",
    [switch]$NoRomfs,
    [switch]$DisableBottomScreen,
    [switch]$NoPause
)

$ErrorActionPreference = "Stop"

try {
    $scriptPath = Join-Path $PSScriptRoot "scripts\build-n3ds.ps1"
    & $scriptPath `
        -BuildDir $BuildDir `
        -Configuration $Configuration `
        -AssetDir $AssetDir `
        -NoRomfs:$NoRomfs `
        -DisableBottomScreen:$DisableBottomScreen
}
catch {
    Write-Host ""
    Write-Host "3DS build failed." -ForegroundColor Red
    Write-Host $_
    exit 1
}
finally {
    if (-not $NoPause) {
        Write-Host ""
        Read-Host "Press Enter to close"
    }
}
