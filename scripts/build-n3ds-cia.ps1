[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [Alias("InputPath")]
    [string]$DataWin,

    [string]$BuildDir = "build\n3ds-cia",
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$AssetDir = "build\game-assets\current",

    [string]$Preprocessor = "",
    [string]$Tex3ds = "",
    [string]$Makerom = "",
    [string]$Bannertool = "",

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
    [switch]$KeepStaging
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function Resolve-InputPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Path))
}

function Resolve-DataWinPath {
    param([string]$Path)
    $resolved = Resolve-InputPath -Path $Path
    if (Test-Path -LiteralPath $resolved -PathType Container) {
        $resolved = Join-Path $resolved "data.win"
    }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "data.win was not found: $resolved"
    }
    return (Resolve-Path -LiteralPath $resolved).Path
}

function Resolve-Executable {
    param(
        [string]$ExplicitPath,
        [string[]]$CommandNames,
        [string[]]$FallbackPaths,
        [string]$Label
    )

    if ($ExplicitPath) {
        $candidate = if ([System.IO.Path]::IsPathRooted($ExplicitPath)) {
            $ExplicitPath
        } else {
            Resolve-RepoPath -Path $ExplicitPath
        }
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
        $command = Get-Command $ExplicitPath -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
        throw "$Label was not found: $ExplicitPath"
    }

    foreach ($name in $CommandNames) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    foreach ($path in $FallbackPaths) {
        if ($path -and (Test-Path -LiteralPath $path -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }
    return $null
}

function Get-ExistingPreprocessor {
    param([string]$ExplicitPath)
    if ($ExplicitPath) {
        return Resolve-Executable -ExplicitPath $ExplicitPath -CommandNames @() -FallbackPaths @() -Label "n3ds-preprocess"
    }

    $candidates = @(
        (Join-Path $repoRoot "build\host-n3ds-preprocess\n3ds-preprocess.exe"),
        (Join-Path $repoRoot "build\host-n3ds-preprocess\Release\n3ds-preprocess.exe"),
        (Join-Path $repoRoot "build\host-n3ds-preprocess\n3ds-preprocess"),
        (Join-Path $repoRoot "build\n3ds-preprocess\n3ds-preprocess.exe"),
        (Join-Path $repoRoot "build\n3ds-preprocess\Release\n3ds-preprocess.exe"),
        (Join-Path $repoRoot "build\n3ds-preprocess\n3ds-preprocess")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Get-SafeFileStem {
    param([string]$Value)
    if (-not $Value) { return "gamemaker-game" }
    $stem = $Value.ToLowerInvariant() -replace '[^a-z0-9._-]+', '-'
    $stem = $stem.Trim('-', '.', '_')
    if (-not $stem) { return "gamemaker-game" }
    if ($stem.Length -gt 48) {
        $stem = $stem.Substring(0, 48).TrimEnd('-', '.', '_')
    }
    if (-not $stem) { return "gamemaker-game" }
    return $stem
}

function Resolve-MetadataAsset {
    param([string]$OverridePath, [string]$PresetPath, [string]$Label)

    $candidate = ""
    if ($OverridePath) {
        $candidate = if ([System.IO.Path]::IsPathRooted($OverridePath)) {
            $OverridePath
        } else {
            Resolve-InputPath -Path $OverridePath
        }
    } elseif ($PresetPath) {
        $candidate = Resolve-RepoPath -Path $PresetPath
    }

    if (-not $candidate) { return $null }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "$Label was not found: $candidate"
    }
    return (Resolve-Path -LiteralPath $candidate).Path
}

function Test-GamePresetMatch {
    param($Preset, $Metadata)

    if ($null -eq $Preset -or $null -eq $Metadata -or $null -eq $Preset.match) {
        return $false
    }

    $displayName = [string]$Metadata.display_name
    $internalName = [string]$Metadata.name
    $steamAppId = [int64]$Metadata.steam_app_id

    foreach ($candidate in @($Preset.match.display_names)) {
        if ($candidate -and $displayName -and $displayName -ieq [string]$candidate) { return $true }
    }
    foreach ($candidate in @($Preset.match.names)) {
        if ($candidate -and $internalName -and $internalName -ieq [string]$candidate) { return $true }
    }
    foreach ($candidate in @($Preset.match.steam_app_ids)) {
        if ($null -ne $candidate -and $steamAppId -ge 0 -and $steamAppId -eq [int64]$candidate) { return $true }
    }

    return $false
}

$dataWinPath = Resolve-DataWinPath -Path $DataWin
$buildPath = Resolve-RepoPath -Path $BuildDir
$assetPath = Resolve-RepoPath -Path $AssetDir
$preprocessorSource = Join-Path $repoRoot "tools\n3ds-preprocess\main.c"
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$preprocessorPath = Get-ExistingPreprocessor -ExplicitPath $Preprocessor
$needsPreprocessorBuild = -not $preprocessorPath
if (-not $needsPreprocessorBuild -and -not $SkipPreprocessorBuild -and (Test-Path -LiteralPath $preprocessorSource -PathType Leaf)) {
    if ((Get-Item -LiteralPath $preprocessorSource).LastWriteTimeUtc -gt (Get-Item -LiteralPath $preprocessorPath).LastWriteTimeUtc) {
        $needsPreprocessorBuild = $true
    }
}

if ($needsPreprocessorBuild) {
    if ($SkipPreprocessorBuild) {
        throw "n3ds-preprocess is missing or stale, but -SkipPreprocessorBuild was specified."
    }
    Write-Host "Building host preprocessor..."
    & (Join-Path $PSScriptRoot "build-preprocessor.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "Host preprocessor build failed with exit code $LASTEXITCODE."
    }
    $preprocessorPath = Get-ExistingPreprocessor -ExplicitPath ""
    if (-not $preprocessorPath) {
        throw "Host preprocessor build succeeded, but n3ds-preprocess could not be located."
    }
}

if (-not $SkipPreprocess) {
    Write-Host ""
    Write-Host "Preprocessing local data.win for Nintendo 3DS..."
    $preprocessArgs = @{
        DataWin = $dataWinPath
        OutputDir = $assetPath
        Preprocessor = $preprocessorPath
        Tex3ds = $Tex3ds
    }
    & (Join-Path $PSScriptRoot "preprocess-game.ps1") @preprocessArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Game preprocessing failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath $assetPath -PathType Container)) {
    throw "Generated asset directory was not found: $assetPath"
}

$metadataPath = Join-Path $assetPath "game_metadata.json"
if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
    throw "GameMaker metadata was not found: $metadataPath. Rebuild the host preprocessor and run again."
}

$metadata = Get-Content -LiteralPath $metadataPath -Raw -Encoding UTF8 | ConvertFrom-Json
$presetPath = Join-Path $repoRoot "resources\3ds\meta\game-presets.json"
$presetDocument = Get-Content -LiteralPath $presetPath -Raw -Encoding UTF8 | ConvertFrom-Json

$preset = $null
if ($PresetId) {
    $preset = @($presetDocument.presets) | Where-Object { $_.id -ieq $PresetId } | Select-Object -First 1
    if (-not $preset) { throw "Unknown game metadata preset: $PresetId" }
} else {
    foreach ($candidate in @($presetDocument.presets)) {
        if (Test-GamePresetMatch -Preset $candidate -Metadata $metadata) {
            $preset = $candidate
            break
        }
    }
}

$detectedTitle = [string]$metadata.display_name
if (-not $detectedTitle) { $detectedTitle = [string]$metadata.name }
if (-not $detectedTitle) { $detectedTitle = [string]$metadata.file_name }
if (-not $detectedTitle) { $detectedTitle = "GameMaker Game" }

$resolvedTitle = if ($Title) { $Title } elseif ($preset -and $preset.title) { [string]$preset.title } else { $detectedTitle }
$resolvedDescription = if ($Description) { $Description } elseif ($preset -and $preset.description) { [string]$preset.description } else { "$resolvedTitle for Nintendo 3DS" }
$resolvedAuthor = if ($Author) { $Author } elseif ($preset -and $preset.author) { [string]$preset.author } else { "Local GameMaker build" }

$gameId = [uint32]$metadata.game_id
$resolvedUniqueId = if ($UniqueId) {
    $UniqueId
} elseif ($preset -and $preset.unique_id) {
    [string]$preset.unique_id
} else {
    "0x{0:X5}" -f (0xC0000 -bor ([int]$gameId -band 0x3FFFF))
}

$resolvedProductCode = if ($ProductCode) {
    $ProductCode
} elseif ($preset -and $preset.product_code) {
    [string]$preset.product_code
} else {
    "CTR-N-{0:X4}" -f ([int]$gameId -band 0xFFFF)
}

$presetIcon = if ($preset -and $preset.icon) { [string]$preset.icon } else { "" }
$presetBannerImage = if ($preset -and $preset.banner_image) { [string]$preset.banner_image } else { "" }
$presetBannerAudio = if ($preset -and $preset.banner_audio) { [string]$preset.banner_audio } else { "" }

$iconPath = Resolve-MetadataAsset -OverridePath $Icon -PresetPath $presetIcon -Label "3DS icon"
$bannerImagePath = Resolve-MetadataAsset -OverridePath $BannerImage -PresetPath $presetBannerImage -Label "3DS banner image"
$bannerAudioPath = Resolve-MetadataAsset -OverridePath $BannerAudio -PresetPath $presetBannerAudio -Label "3DS banner audio"

if (-not $iconPath -or -not $bannerImagePath -or -not $bannerAudioPath) {
    throw "No complete 3DS metadata preset matched '$detectedTitle'. Add a preset or pass -Icon, -BannerImage, and -BannerAudio."
}

Write-Host ""
Write-Host "Game metadata:"
Write-Host "  Detected:     $detectedTitle"
Write-Host "  Preset:       $(if ($preset) { $preset.id } else { '(custom)' })"
Write-Host "  Home title:   $resolvedTitle"
Write-Host "  Product code: $resolvedProductCode"
Write-Host "  Unique ID:    $resolvedUniqueId"

$romfsPath = Join-Path $buildPath "romfs"
$appBuildPath = Join-Path $buildPath "app"
$packagePath = Join-Path $buildPath "package"
$ciaPath = Join-Path $buildPath ((Get-SafeFileStem -Value $resolvedTitle) + ".cia")
$rsfPath = Join-Path $repoRoot "resources\3ds\meta\template.rsf"

$devkitPro = if ($env:DEVKITPRO) { $env:DEVKITPRO } else { "C:\devkitPro" }
$makeromPath = Resolve-Executable -ExplicitPath $Makerom -CommandNames @("makerom.exe", "makerom") -FallbackPaths @(
    (Join-Path $devkitPro "tools\bin\makerom.exe"),
    (Join-Path $devkitPro "tools\bin\makerom")
) -Label "makerom"
$bannertoolPath = Resolve-Executable -ExplicitPath $Bannertool -CommandNames @("bannertool.exe", "bannertool") -FallbackPaths @(
    (Join-Path $devkitPro "tools\bin\bannertool.exe"),
    (Join-Path $devkitPro "tools\bin\bannertool")
) -Label "bannertool"

if (-not $makeromPath) {
    throw "makerom was not found. Add it to PATH or pass -Makerom <path>."
}
if (-not $bannertoolPath) {
    throw "bannertool was not found. Add it to PATH or pass -Bannertool <path>."
}
if (-not (Test-Path -LiteralPath $rsfPath -PathType Leaf)) {
    throw "CIA RSF template was not found: $rsfPath"
}

$ciaSucceeded = $false
try {
    if (Test-Path -LiteralPath $romfsPath) {
        Remove-Item -LiteralPath $romfsPath -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $romfsPath | Out-Null

    foreach ($item in Get-ChildItem -LiteralPath $assetPath -Force) {
        if ($item.Name -ieq "game_metadata.json" -or $item.Name -ieq "data.win") { continue }
        Copy-Item -LiteralPath $item.FullName -Destination $romfsPath -Recurse -Force
    }
    Copy-Item -LiteralPath $dataWinPath -Destination (Join-Path $romfsPath "data.win") -Force

    Write-Host ""
    Write-Host "Building self-contained Nintendo 3DS executable..."
    $n3dsArgs = @{
        BuildDir = $appBuildPath
        Configuration = $Configuration
        AssetDir = $romfsPath
        DisableBottomScreen = $DisableBottomScreen
    }
    & (Join-Path $PSScriptRoot "build-n3ds.ps1") @n3dsArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Nintendo 3DS ELF build failed with exit code $LASTEXITCODE."
    }

    $elfPath = Join-Path $appBuildPath "cinnamon.elf"
    if (-not (Test-Path -LiteralPath $elfPath -PathType Leaf)) {
        $elfPath = Get-ChildItem -LiteralPath $appBuildPath -Recurse -File -Filter "cinnamon.elf" | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $elfPath -or -not (Test-Path -LiteralPath $elfPath -PathType Leaf)) {
        throw "cinnamon.elf was not found under $appBuildPath."
    }
    $elfPath = (Resolve-Path -LiteralPath $elfPath).Path

    if (Test-Path -LiteralPath $packagePath) {
        Remove-Item -LiteralPath $packagePath -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packagePath | Out-Null

    $iconIcn = Join-Path $packagePath "icon.icn"
    $bannerBnr = Join-Path $packagePath "banner.bnr"

    Write-Host ""
    Write-Host "Creating Home Menu metadata..."
    & $bannertoolPath makesmdh -s $resolvedTitle -l $resolvedDescription -p $resolvedAuthor -i $iconPath -o $iconIcn
    if ($LASTEXITCODE -ne 0) { throw "bannertool makesmdh failed with exit code $LASTEXITCODE." }

    & $bannertoolPath makebanner -i $bannerImagePath -a $bannerAudioPath -o $bannerBnr
    if ($LASTEXITCODE -ne 0) { throw "bannertool makebanner failed with exit code $LASTEXITCODE." }

    if (Test-Path -LiteralPath $ciaPath -PathType Leaf) {
        Remove-Item -LiteralPath $ciaPath -Force
    }

    $romfsMakeromPath = ([System.IO.Path]::GetFullPath($romfsPath)).Replace('\', '/')
    $makeromArgs = @(
        "-f", "cia",
        "-o", $ciaPath,
        "-target", "t",
        "-exefslogo",
        "-elf", $elfPath,
        "-rsf", $rsfPath,
        "-icon", $iconIcn,
        "-banner", $bannerBnr,
        "-DAPP_TITLE=$resolvedTitle",
        "-DAPP_PRODUCT_CODE=$resolvedProductCode",
        "-DAPP_UNIQUE_ID=$resolvedUniqueId",
        "-DAPP_ENCRYPTED=false",
        "-DAPP_ROMFS=$romfsMakeromPath",
        "-DAPP_SYSTEM_MODE=64MB",
        "-DAPP_SYSTEM_MODE_EXT=Legacy",
        "-DAPP_CPU_SPEED=804MHz",
        "-DAPP_ENABLE_L2_CACHE=true",
        "-DAPP_CAN_ACCESS_CORE2=false",
        "-major", "0",
        "-minor", "2",
        "-micro", "0"
    )

    Write-Host ""
    Write-Host "Packaging local self-contained CIA..."
    Write-Host "  makerom: $makeromPath"
    Write-Host "  RomFS:   $romfsPath"
    & $makeromPath @makeromArgs
    if ($LASTEXITCODE -ne 0) { throw "makerom failed with exit code $LASTEXITCODE." }
    if (-not (Test-Path -LiteralPath $ciaPath -PathType Leaf)) {
        throw "makerom returned success, but the CIA was not created: $ciaPath"
    }

    $ciaHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ciaPath).Hash.ToLowerInvariant()
    $dataWinHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $dataWinPath).Hash.ToLowerInvariant()
    $elfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $elfPath).Hash.ToLowerInvariant()

    $manifestDir = Join-Path $repoRoot "build\repro"
    New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null
    $manifestPath = Join-Path $manifestDir "n3ds-cia-build.json"

    $manifest = [ordered]@{
        format = "local-self-contained-cia"
        detected_game = [ordered]@{
            display_name = $metadata.display_name
            name = $metadata.name
            game_id = $metadata.game_id
            steam_app_id = $metadata.steam_app_id
            bytecode_version = $metadata.bytecode_version
        }
        preset = $(if ($preset) { [string]$preset.id } else { $null })
        home_menu = [ordered]@{
            title = $resolvedTitle
            description = $resolvedDescription
            author = $resolvedAuthor
            product_code = $resolvedProductCode
            unique_id = $resolvedUniqueId
        }
        configuration = $Configuration
        data_win_sha256 = $dataWinHash
        elf = $elfPath
        elf_sha256 = $elfHash
        cia = (Resolve-Path -LiteralPath $ciaPath).Path
        cia_sha256 = $ciaHash
        makerom = $makeromPath
        bannertool = $bannertoolPath
        generated_utc = [DateTime]::UtcNow.ToString("o")
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    $ciaSucceeded = $true
    Write-Host ""
    Write-Host "Nintendo 3DS CIA build completed." -ForegroundColor Green
    Write-Host "CIA:      $ciaPath"
    Write-Host "SHA-256:  $ciaHash"
    Write-Host "Title:    $resolvedTitle"
    Write-Host "Preset:   $(if ($preset) { $preset.id } else { '(custom)' })"
    Write-Host "Manifest: $manifestPath"
}
finally {
    if (-not $KeepStaging -and (Test-Path -LiteralPath $romfsPath)) {
        Remove-Item -LiteralPath $romfsPath -Recurse -Force
        if ($ciaSucceeded) {
            Write-Host "Removed temporary RomFS staging (including copied data.win)."
        } else {
            Write-Host "Removed temporary RomFS staging after failure."
        }
    }
}
