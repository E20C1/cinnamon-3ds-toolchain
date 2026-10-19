[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [Alias("InputPath")]
    [string]$DataWin,

    [string]$BuildDir = "build\n3ds-cia",
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$AssetDir = "",

    [string]$Preprocessor = "",
    [string]$Tex3ds = "",
    [string]$Makerom = "",
    [string]$Bannertool = "",
    [string]$DspFirm = "",

    [string]$PresetId = "",
    [string]$Title = "",
    [string]$Description = "",
    [string]$Author = "",
    [string]$ProductCode = "",
    [string]$UniqueId = "",
    [string]$Icon = "",
    [string]$BannerImage = "",
    [string]$BannerAudio = "",

    [switch]$DisableBottomScreen,
    [switch]$SkipPreprocessorBuild,
    [switch]$SkipPreprocess,
    [switch]$KeepStaging,
    [switch]$NoToolDownload,
    [switch]$NoPause
)

$ErrorActionPreference = "Stop"

try {
    $scriptPath = Join-Path $PSScriptRoot "scripts\build-n3ds-cia.ps1"
    $argsMap = @{
        DataWin = $DataWin
        BuildDir = $BuildDir
        Configuration = $Configuration
        AssetDir = $AssetDir
        Preprocessor = $Preprocessor
        Tex3ds = $Tex3ds
        Makerom = $Makerom
        Bannertool = $Bannertool
        DspFirm = $DspFirm
        PresetId = $PresetId
        Title = $Title
        Description = $Description
        Author = $Author
        ProductCode = $ProductCode
        UniqueId = $UniqueId
        Icon = $Icon
        BannerImage = $BannerImage
        BannerAudio = $BannerAudio
        DisableBottomScreen = $DisableBottomScreen
        SkipPreprocessorBuild = $SkipPreprocessorBuild
        SkipPreprocess = $SkipPreprocess
        KeepStaging = $KeepStaging
        NoToolDownload = $NoToolDownload
    }

    & $scriptPath @argsMap
}
catch {
    Write-Host ""
    Write-Host "3DS CIA build failed." -ForegroundColor Red
    Write-Host $_
    exit 1
}
finally {
    if (-not $NoPause) {
        Write-Host ""
        Read-Host "Press Enter to close"
    }
}
