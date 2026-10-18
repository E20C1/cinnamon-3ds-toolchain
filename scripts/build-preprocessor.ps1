[CmdletBinding()]
param(
    [string]$BuildDir = "build\host-n3ds-preprocess",
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$CMakeExe = "",
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot "tools\n3ds-preprocess"
if (-not (Test-Path -LiteralPath $sourceDir -PathType Container)) {
    throw "n3ds-preprocess source directory was not found: $sourceDir"
}

if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}

function Resolve-Executable {
    param(
        [string]$ExplicitPath,
        [string[]]$CommandNames,
        [string[]]$FallbackPaths
    )

    if ($ExplicitPath) {
        if (Test-Path -LiteralPath $ExplicitPath -PathType Leaf) {
            return (Resolve-Path -LiteralPath $ExplicitPath).Path
        }
        $command = Get-Command $ExplicitPath -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
        throw "Executable was not found: $ExplicitPath"
    }

    foreach ($name in $CommandNames) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }

    foreach ($path in $FallbackPaths) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    return $null
}

$cmake = Resolve-Executable `
    -ExplicitPath $CMakeExe `
    -CommandNames @("cmake.exe", "cmake") `
    -FallbackPaths @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\devkitPro\msys2\usr\bin\cmake.exe"
    )

if (-not $cmake) {
    throw "CMake was not found. Install CMake or pass -CMakeExe <path>."
}

if (-not $Generator) {
    if ($env:CINNAMON_HOST_CMAKE_GENERATOR) {
        $Generator = $env:CINNAMON_HOST_CMAKE_GENERATOR
    } elseif (Get-Command ninja.exe -ErrorAction SilentlyContinue) {
        $Generator = "Ninja"
    } elseif (Get-Command ninja -ErrorAction SilentlyContinue) {
        $Generator = "Ninja"
    } elseif (Get-Command mingw32-make.exe -ErrorAction SilentlyContinue) {
        $Generator = "MinGW Makefiles"
    } elseif (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
        $Generator = "MinGW Makefiles"
    }
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$Configuration"
)
if ($Generator) {
    $configureArgs += @("-G", $Generator)
}

Write-Host "Configuring n3ds-preprocess..."
Write-Host "  CMake: $cmake"
Write-Host "  Source: $sourceDir"
Write-Host "  Build:  $BuildDir"
if ($Generator) {
    Write-Host "  Generator: $Generator"
} else {
    Write-Warning "No host generator was selected explicitly; CMake will use its platform default."
}

& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

& $cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}

$candidates = @(
    (Join-Path $BuildDir "n3ds-preprocess.exe"),
    (Join-Path $BuildDir "$Configuration\n3ds-preprocess.exe"),
    (Join-Path $BuildDir "n3ds-preprocess")
)
$preprocessor = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $preprocessor) {
    $preprocessor = Get-ChildItem -LiteralPath $BuildDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "n3ds-preprocess.exe" -or $_.Name -eq "n3ds-preprocess" } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $preprocessor) {
    throw "The build completed but n3ds-preprocess could not be located under $BuildDir."
}

$preprocessor = (Resolve-Path -LiteralPath $preprocessor).Path
$preprocessorHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $preprocessor).Hash.ToLowerInvariant()

$gitCommit = $null
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
    $gitCommit = (& $git.Source -C $repoRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0) { $gitCommit = $null }
}

$cmakeVersion = (& $cmake --version | Select-Object -First 1)
$compiler = $null
$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path -LiteralPath $cachePath) {
    $compilerLine = Get-Content -LiteralPath $cachePath |
        Where-Object { $_ -match '^CMAKE_C_COMPILER:(FILEPATH|STRING)=' } |
        Select-Object -First 1
    if ($compilerLine) {
        $compiler = ($compilerLine -split '=', 2)[1]
    }
}

$manifestDir = Join-Path $repoRoot "build\repro"
New-Item -ItemType Directory -Force -Path $manifestDir | Out-Null
$manifestPath = Join-Path $manifestDir "preprocessor-build.json"

$manifest = [ordered]@{
    repository_commit = $gitCommit
    configuration = $Configuration
    generator = $Generator
    cmake = $cmake
    cmake_version = $cmakeVersion
    c_compiler = $compiler
    executable = $preprocessor
    executable_sha256 = $preprocessorHash
    generated_utc = [DateTime]::UtcNow.ToString("o")
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host ""
Write-Host "n3ds-preprocess build completed." -ForegroundColor Green
Write-Host "Executable: $preprocessor"
Write-Host "SHA-256:   $preprocessorHash"
Write-Host "Manifest:  $manifestPath"
