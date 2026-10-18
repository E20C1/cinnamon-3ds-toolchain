[CmdletBinding()]
param(
    [string]$BuildDir = "build\n3ds",

    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [string]$AssetDir = "build\game-assets\current",

    [switch]$NoRomfs,

    [switch]$DisableBottomScreen
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

function Require-File {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label was not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Convert-ToMsysPath {
    param([string]$WindowsPath, [string]$CygpathExe)
    $converted = (& $CygpathExe -u $WindowsPath)
    if ($LASTEXITCODE -ne 0 -or -not $converted) {
        throw "cygpath failed to convert: $WindowsPath"
    }
    return ($converted | Select-Object -First 1).Trim()
}

function Quote-Bash {
    param([string]$Text)
    return "'" + $Text.Replace("'", "'\''") + "'"
}

function Get-TreeFingerprint {
    param([string]$Root)

    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $records = New-Object System.Collections.Generic.List[string]
    $files = @(Get-ChildItem -LiteralPath $rootFull -Recurse -File | Sort-Object FullName)

    foreach ($file in $files) {
        $relative = $file.FullName.Substring($rootFull.Length).TrimStart('\', '/').Replace('\', '/')
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        $records.Add($relative + [char]9 + $hash + [char]10)
    }

    $payload = [System.Text.Encoding]::UTF8.GetBytes(($records -join ""))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($payload)
    } finally {
        $sha.Dispose()
    }

    return [ordered]@{
        file_count = $files.Count
        sha256 = (($digest | ForEach-Object { $_.ToString("x2") }) -join "")
    }
}

function Verify-PreprocessManifest {
    param(
        [string]$AssetRoot,
        [string]$ManifestPath,
        [string[]]$RequiredOutputs
    )

    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        Write-Warning "Preprocess manifest was not found; continuing with direct asset validation only: $ManifestPath"
        return $null
    }

    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    foreach ($relative in $RequiredOutputs) {
        $normalized = $relative.Replace('\', '/')
        $property = $manifest.required_output_sha256.PSObject.Properties[$normalized]
        if (-not $property) {
            throw "Preprocess manifest does not contain the required output hash: $normalized"
        }

        $assetFile = Join-Path $AssetRoot $relative
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $assetFile).Hash.ToLowerInvariant()
        $expected = ([string]$property.Value).ToLowerInvariant()
        if ($actual -ne $expected) {
            throw "Asset hash differs from last-preprocess.json for $($normalized). Expected $expected, got $actual."
        }
    }

    Write-Host "[OK] Generated assets match build\repro\last-preprocess.json."
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $ManifestPath).Hash.ToLowerInvariant()
}

$devkitPro = "C:\devkitPro"
$bash = Require-File -Path (Join-Path $devkitPro "msys2\usr\bin\bash.exe") -Label "devkitPro MSYS2 bash"
$cygpath = Require-File -Path (Join-Path $devkitPro "msys2\usr\bin\cygpath.exe") -Label "devkitPro cygpath"
$cmake = Require-File -Path (Join-Path $devkitPro "msys2\usr\bin\cmake.exe") -Label "devkitPro MSYS2 CMake"
$make = Require-File -Path (Join-Path $devkitPro "msys2\usr\bin\make.exe") -Label "devkitPro make"
$toolchain = Require-File -Path (Join-Path $devkitPro "cmake\3DS.cmake") -Label "devkitPro 3DS CMake toolchain"
$armCmake = Require-File -Path (Join-Path $devkitPro "portlibs\3ds\bin\arm-none-eabi-cmake") -Label "devkitPro 3DS CMake wrapper"
$armGcc = Require-File -Path (Join-Path $devkitPro "devkitARM\bin\arm-none-eabi-gcc.exe") -Label "devkitARM GCC"
$threeDsxTool = Require-File -Path (Join-Path $devkitPro "tools\bin\3dsxtool.exe") -Label "3dsxtool"
$smdhTool = Require-File -Path (Join-Path $devkitPro "tools\bin\smdhtool.exe") -Label "smdhtool"

$buildPath = Resolve-RepoPath -Path $BuildDir
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$requiredOutputs = @(
    "gfx\atlas.bin",
    "gfx\direct_assets.bin",
    "audio\sound_bank.bin"
)

$assetPath = $null
$assetFingerprint = $null
$preprocessManifestHash = $null
if (-not $NoRomfs) {
    $assetPath = Resolve-RepoPath -Path $AssetDir
    if (-not (Test-Path -LiteralPath $assetPath -PathType Container)) {
        throw "Generated 3DS asset directory was not found: $assetPath`nRun scripts\preprocess-game.ps1 first, or pass -AssetDir <path>."
    }
    $assetPath = (Resolve-Path -LiteralPath $assetPath).Path

    foreach ($relative in $requiredOutputs) {
        $requiredPath = Join-Path $assetPath $relative
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Required preprocessed asset is missing: $requiredPath"
        }
    }

    $preprocessManifestPath = Join-Path $repoRoot "build\repro\last-preprocess.json"
    $preprocessManifestHash = Verify-PreprocessManifest `
        -AssetRoot $assetPath `
        -ManifestPath $preprocessManifestPath `
        -RequiredOutputs $requiredOutputs

    $assetFingerprint = Get-TreeFingerprint -Root $assetPath
}

$repoMsys = Convert-ToMsysPath -WindowsPath $repoRoot -CygpathExe $cygpath
$buildMsys = Convert-ToMsysPath -WindowsPath $buildPath -CygpathExe $cygpath
$toolchainMsys = Convert-ToMsysPath -WindowsPath $toolchain -CygpathExe $cygpath
$assetMsys = if ($assetPath) { Convert-ToMsysPath -WindowsPath $assetPath -CygpathExe $cygpath } else { "" }

$gitCommit = $null
$gitCommitDate = $null
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
    $gitCommit = (& $git.Source -C $repoRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0) { $gitCommit = $null }
    $gitCommitDate = (& $git.Source -C $repoRoot show -s --format=%cI HEAD 2>$null)
    if ($LASTEXITCODE -ne 0) { $gitCommitDate = $null }
}

foreach ($name in @("cinnamon.3dsx", "cinnamon.elf", "cinnamon.default.smdh")) {
    $candidate = Join-Path $buildPath $name
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        Remove-Item -LiteralPath $candidate -Force
    }
}

$cmakeArgs = @(
    "--fresh",
    "-S", (Quote-Bash $repoMsys),
    "-B", (Quote-Bash $buildMsys),
    "-G", (Quote-Bash "Unix Makefiles"),
    "-DPLATFORM=n3ds",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_TOOLCHAIN_FILE=$(Quote-Bash $toolchainMsys)",
    "-DN3DS_DISABLE_BOTTOM_SCREEN=$(if ($DisableBottomScreen) { 'ON' } else { 'OFF' })"
)

if ($assetPath) {
    $cmakeArgs += "-DN3DS_ROMFS_DIR=$(Quote-Bash $assetMsys)"
} else {
    $cmakeArgs += "-DN3DS_ROMFS_DIR="
}

if ($gitCommit) {
    $cmakeArgs += "-DBUTTERSCOTCH_COMMIT_HASH=$(Quote-Bash $gitCommit)"
}
if ($gitCommitDate) {
    $cmakeArgs += "-DBUTTERSCOTCH_COMMIT_DATE=$(Quote-Bash $gitCommitDate)"
}

$configureCommand = "/usr/bin/cmake " + ($cmakeArgs -join " ")
$buildCommand = "/usr/bin/cmake --build $(Quote-Bash $buildMsys)"

$command = @"
set -e
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export PATH="/opt/devkitpro/portlibs/3ds/bin:/opt/devkitpro/devkitARM/bin:/opt/devkitpro/tools/bin:/usr/bin:`$PATH"
cd $(Quote-Bash $repoMsys)
echo "3DS compiler: `$(arm-none-eabi-gcc --version | head -n 1)"
echo "3DS CMake: `$(cmake --version | head -n 1)"
$configureCommand
$buildCommand
"@

Write-Host "Building Cinnamon for Nintendo 3DS..."
Write-Host "  Build:         $buildPath"
Write-Host "  Configuration: $Configuration"
Write-Host "  Toolchain:     $toolchain"
Write-Host "  Compiler:      $armGcc"
if ($assetPath) {
    Write-Host "  RomFS assets:  $assetPath"
    Write-Host "  Asset files:   $($assetFingerprint.file_count)"
    Write-Host "  Asset SHA-256: $($assetFingerprint.sha256)"
} else {
    Write-Host "  RomFS assets:  disabled (runtime SD assets required)"
}

& $bash -lc $command
if ($LASTEXITCODE -ne 0) {
    throw "Nintendo 3DS build failed with exit code $LASTEXITCODE."
}

$threeDsxCandidates = @(
    (Join-Path $buildPath "cinnamon.3dsx")
)
$threeDsx = $threeDsxCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $threeDsx) {
    $threeDsx = Get-ChildItem -LiteralPath $buildPath -Recurse -File -Filter "cinnamon.3dsx" |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $threeDsx) {
    throw "The 3DS build returned success, but cinnamon.3dsx was not found under $buildPath."
}
$threeDsx = (Resolve-Path -LiteralPath $threeDsx).Path

$elf = Get-ChildItem -LiteralPath $buildPath -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ieq "cinnamon.elf" } |
    Select-Object -First 1 -ExpandProperty FullName

$threeDsxHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $threeDsx).Hash.ToLowerInvariant()
$elfHash = if ($elf) { (Get-FileHash -Algorithm SHA256 -LiteralPath $elf).Hash.ToLowerInvariant() } else { $null }

$manifestDir = Join-Path $repoRoot "build\repro"
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null
$manifestPath = Join-Path $manifestDir "n3ds-build.json"

$manifest = [ordered]@{
    repository_commit = $gitCommit
    repository_commit_date = $gitCommitDate
    configuration = $Configuration
    platform = "n3ds"
    initial_hardware_validation_target = "New Nintendo 3DS family"
    devkitpro_root = $devkitPro
    cmake = $cmake
    make = $make
    toolchain = $toolchain
    arm_cmake_wrapper = $armCmake
    arm_gcc = $armGcc
    threedstool = $threeDsxTool
    smdhtool = $smdhTool
    romfs_enabled = (-not $NoRomfs)
    romfs_asset_dir = $assetPath
    romfs_asset_file_count = $(if ($assetFingerprint) { $assetFingerprint.file_count } else { 0 })
    romfs_asset_tree_sha256 = $(if ($assetFingerprint) { $assetFingerprint.sha256 } else { $null })
    preprocess_manifest_sha256 = $preprocessManifestHash
    cinnamon_3dsx = $threeDsx
    cinnamon_3dsx_sha256 = $threeDsxHash
    cinnamon_elf = $elf
    cinnamon_elf_sha256 = $elfHash
    generated_utc = [DateTime]::UtcNow.ToString("o")
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host ""
Write-Host "Nintendo 3DS build completed." -ForegroundColor Green
Write-Host "3DSX:     $threeDsx"
Write-Host "SHA-256:  $threeDsxHash"
if ($elf) {
    Write-Host "ELF:      $elf"
    Write-Host "ELF SHA:  $elfHash"
}
Write-Host "Manifest: $manifestPath"
