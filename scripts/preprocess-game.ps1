[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [Alias("InputPath")]
    [string]$DataWin,

    [string]$OutputDir = "build\game-assets\current",

    [string]$Preprocessor = "",

    [string]$Tex3ds = "",

    [ValidateSet("auto", "256", "512")]
    [string]$AtlasPageMode = "auto",

    [ValidateSet("etc1a4", "rgba5551", "indexed8", "hybrid")]
    [string]$TextureFormat = "hybrid",

    [switch]$KeepPng,
    [switch]$DumpPagePreviews
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-LocalPath {
    param([string]$Path, [string]$Base)
    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $Base $Path)
}

function Get-ExternalAudioFingerprint {
    param([string]$DataWinPath)

    $root = Split-Path -Parent $DataWinPath
    $audioExtensions = @(".ogg", ".oga", ".wav")
    $files = @(
        Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $audioExtensions -contains $_.Extension.ToLowerInvariant() } |
            Sort-Object FullName
    )

    $builder = New-Object System.Text.StringBuilder
    [int64]$totalBytes = 0
    foreach ($file in $files) {
        $trimChars = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
        $relative = $file.FullName.Substring($root.Length).TrimStart($trimChars)
        $relative = $relative.Replace([System.IO.Path]::DirectorySeparatorChar, [char]'/')
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
        [void]$builder.Append($relative)
        [void]$builder.Append('|')
        [void]$builder.Append([string]$file.Length)
        [void]$builder.Append('|')
        [void]$builder.Append($hash)
        [void]$builder.Append([Environment]::NewLine)
        $totalBytes += [int64]$file.Length
    }

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($builder.ToString())
        $digest = $sha.ComputeHash($bytes)
        $fingerprint = ([System.BitConverter]::ToString($digest)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }

    return [ordered]@{
        file_count = $files.Count
        total_bytes = $totalBytes
        sha256 = $fingerprint
    }
}

function Resolve-Executable {
    param(
        [string]$ExplicitPath,
        [string[]]$Candidates
    )

    if ($ExplicitPath) {
        $candidatePath = Resolve-LocalPath -Path $ExplicitPath -Base $repoRoot
        if (Test-Path -LiteralPath $candidatePath -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
        $command = Get-Command $ExplicitPath -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
        throw "Executable was not found: $ExplicitPath"
    }

    foreach ($candidate in $Candidates) {
        $candidatePath = Resolve-LocalPath -Path $candidate -Base $repoRoot
        if (Test-Path -LiteralPath $candidatePath -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
    }

    return $null
}

$dataWinPath = Resolve-LocalPath -Path $DataWin -Base (Get-Location).Path
if (Test-Path -LiteralPath $dataWinPath -PathType Container) {
    $directoryInput = (Resolve-Path -LiteralPath $dataWinPath).Path
    $candidate = Join-Path $directoryInput "data.win"
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        Write-Host "DataWin points to a directory; using: $candidate"
        $dataWinPath = $candidate
    } else {
        throw "DataWin points to a directory, but data.win was not found inside it: $directoryInput"
    }
}

if (-not (Test-Path -LiteralPath $dataWinPath -PathType Leaf)) {
    throw "data.win was not found: $dataWinPath"
}
$dataWinPath = (Resolve-Path -LiteralPath $dataWinPath).Path

$outputPath = Resolve-LocalPath -Path $OutputDir -Base $repoRoot
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$outputPath = (Resolve-Path -LiteralPath $outputPath).Path

$preprocessorPath = Resolve-Executable -ExplicitPath $Preprocessor -Candidates @(
    "build\host-n3ds-preprocess\n3ds-preprocess.exe",
    "build\host-n3ds-preprocess\Release\n3ds-preprocess.exe",
    "build\host-n3ds-preprocess\n3ds-preprocess",
    "build\n3ds-preprocess\n3ds-preprocess.exe",
    "build\n3ds-preprocess\Release\n3ds-preprocess.exe",
    "build\n3ds-preprocess\n3ds-preprocess"
)
if (-not $preprocessorPath) {
    throw "n3ds-preprocess was not found. Run scripts\build-preprocessor.ps1 first, or pass -Preprocessor <path>."
}

if ($Tex3ds) {
    $tex3dsPath = Resolve-Executable -ExplicitPath $Tex3ds -Candidates @()
} else {
    $tex3dsCandidates = @()
    if ($env:DEVKITPRO) {
        $tex3dsCandidates += (Join-Path $env:DEVKITPRO "tools\bin\tex3ds.exe")
        $tex3dsCandidates += (Join-Path $env:DEVKITPRO "tools\bin\tex3ds")
    }
    $tex3dsCandidates += "C:\devkitPro\tools\bin\tex3ds.exe"
    $tex3dsCandidates += "C:\devkitPro\tools\bin\tex3ds"

    $tex3dsPath = $null
    $tex3dsCommand = Get-Command tex3ds.exe -ErrorAction SilentlyContinue
    if (-not $tex3dsCommand) { $tex3dsCommand = Get-Command tex3ds -ErrorAction SilentlyContinue }
    if ($tex3dsCommand) {
        $tex3dsPath = $tex3dsCommand.Source
    } else {
        foreach ($candidate in $tex3dsCandidates) {
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                $tex3dsPath = (Resolve-Path -LiteralPath $candidate).Path
                break
            }
        }
    }
}
if (-not $tex3dsPath) {
    throw "tex3ds was not found. Install the devkitPro 3DS tools, set DEVKITPRO, or pass -Tex3ds <path>."
}

$args = @(
    $dataWinPath,
    $outputPath,
    "--tex3ds", $tex3dsPath,
    "--atlas-page-mode", $AtlasPageMode,
    "--texture-format", $TextureFormat
)
if ($KeepPng) { $args += "--keep-png" }
if ($DumpPagePreviews) { $args += "--dump-page-previews" }

Write-Host "Running n3ds-preprocess..."
Write-Host "  Input:       $dataWinPath"
Write-Host "  Output:      $outputPath"
Write-Host "  Preprocess:  $preprocessorPath"
Write-Host "  tex3ds:      $tex3dsPath"
Write-Host "  Atlas mode:  $AtlasPageMode"
Write-Host "  Texture fmt: $TextureFormat"

& $preprocessorPath @args
if ($LASTEXITCODE -ne 0) {
    throw "n3ds-preprocess failed with exit code $LASTEXITCODE."
}

$requiredOutputs = @(
    "gfx\atlas.bin",
    "gfx\direct_assets.bin",
    "audio\sound_bank.bin",
    "game_metadata.json"
)
$missing = @()
foreach ($relative in $requiredOutputs) {
    $path = Join-Path $outputPath $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $missing += $relative
    }
}
if ($missing.Count -gt 0) {
    throw "Preprocessing returned success but required outputs are missing: $($missing -join ', ')"
}

$gitCommit = $null
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
    $gitCommit = (& $git.Source -C $repoRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0) { $gitCommit = $null }
}

$inputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $dataWinPath).Hash.ToLowerInvariant()
$externalAudioFingerprint = Get-ExternalAudioFingerprint -DataWinPath $dataWinPath
$preprocessorHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $preprocessorPath).Hash.ToLowerInvariant()

$outputHashes = [ordered]@{}
foreach ($relative in $requiredOutputs) {
    $path = Join-Path $outputPath $relative
    $outputHashes[$relative.Replace("\", "/")] = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
}

$manifestDir = Join-Path $repoRoot "build\repro"
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null
$manifestPath = Join-Path $manifestDir "last-preprocess.json"

$manifest = [ordered]@{
    repository_commit = $gitCommit
    input_sha256 = $inputHash
    external_audio = $externalAudioFingerprint
    preprocessor_sha256 = $preprocessorHash
    atlas_page_mode = $AtlasPageMode
    texture_format = $TextureFormat
    required_output_sha256 = $outputHashes
    generated_utc = [DateTime]::UtcNow.ToString("o")
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

$sourceManifestPath = Join-Path $outputPath "n3ds_source_manifest.json"
$sourceManifest = [ordered]@{
    schema_version = 2
    data_win_sha256 = $inputHash
    external_audio = $externalAudioFingerprint
    preprocessor_sha256 = $preprocessorHash
    generated_utc = [DateTime]::UtcNow.ToString("o")
}
$sourceManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $sourceManifestPath -Encoding UTF8

Write-Host ""
Write-Host "Preprocessing completed." -ForegroundColor Green
Write-Host "Output:   $outputPath"
Write-Host "Manifest: $manifestPath"
Write-Host "Audio files: $($externalAudioFingerprint.file_count) ($($externalAudioFingerprint.total_bytes) bytes)"
