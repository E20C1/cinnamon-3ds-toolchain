[CmdletBinding()]
param(
    [string]$BuildDir = "build\host-n3ds-preprocess",
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$CMakeExe = "",
    [string]$Generator = "",
    [string]$HostCompiler = "",
    [string]$MakeProgram = ""
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

function Resolve-HostCMake {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        return Resolve-Executable -ExplicitPath $ExplicitPath -CommandNames @() -FallbackPaths @()
    }

    $nativeCandidates = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\CMake\bin\cmake.exe"
    )

    foreach ($path in $nativeCandidates) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            return (Resolve-Path -LiteralPath $path).Path
        }
    }

    $commands = @(Get-Command cmake.exe, cmake -All -ErrorAction SilentlyContinue)
    foreach ($command in $commands) {
        if (-not $command) { continue }
        $source = $command.Source
        if ($source -and $source -notmatch '(?i)\\devkitPro\\msys2\\') {
            return $source
        }
    }

    return $null
}

$cmake = Resolve-HostCMake -ExplicitPath $CMakeExe

if (-not $cmake) {
    throw @"
A native Windows CMake executable was not found.

The devkitPro-bundled MSYS2 CMake is intentionally not used for host builds
because it interprets native C:\... paths as MSYS paths. Install Kitware CMake
or pass -CMakeExe <native-windows-cmake.exe>.
"@
}

$compiler = Resolve-Executable `
    -ExplicitPath $HostCompiler `
    -CommandNames @("gcc.exe", "gcc", "clang.exe", "clang") `
    -FallbackPaths @(
        "C:\msys64\ucrt64\bin\gcc.exe",
        "C:\devkitPro\msys2\ucrt64\bin\gcc.exe",
        "C:\devkitPro\msys2\mingw64\bin\gcc.exe",
        "C:\devkitPro\msys2\clang64\bin\clang.exe"
    )

if (-not $compiler) {
    throw @"
A Windows host C compiler was not found.

devkitPro's bundled MSYS2 does not necessarily expose the full upstream
MSYS2 UCRT64 package repositories. Install a standard MSYS2 instance
(default: C:\msys64), then install the UCRT64 host toolchain there:

  & "C:\msys64\usr\bin\pacman.exe" -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja

Then rerun this script. You may also pass -HostCompiler <path>.
"@
}

$makeTool = Resolve-Executable `
    -ExplicitPath $MakeProgram `
    -CommandNames @("ninja.exe", "ninja", "mingw32-make.exe", "mingw32-make") `
    -FallbackPaths @(
        "C:\msys64\ucrt64\bin\ninja.exe",
        "C:\msys64\ucrt64\bin\mingw32-make.exe",
        "C:\devkitPro\msys2\ucrt64\bin\ninja.exe",
        "C:\devkitPro\msys2\mingw64\bin\ninja.exe",
        "C:\devkitPro\msys2\ucrt64\bin\mingw32-make.exe",
        "C:\devkitPro\msys2\mingw64\bin\mingw32-make.exe"
    )

if (-not $Generator) {
    if ($env:CINNAMON_HOST_CMAKE_GENERATOR) {
        $Generator = $env:CINNAMON_HOST_CMAKE_GENERATOR
    } elseif ($makeTool -and ([System.IO.Path]::GetFileName($makeTool) -match '^ninja(\.exe)?$')) {
        $Generator = "Ninja"
    } elseif ($makeTool -and ([System.IO.Path]::GetFileName($makeTool) -match '^mingw32-make(\.exe)?$')) {
        $Generator = "MinGW Makefiles"
    }
}

if (-not $Generator) {
    throw "A supported host build generator was not found. Install Ninja or pass -Generator and -MakeProgram explicitly."
}

# MSYS2 MinGW/UCRT compilers rely on sibling tools/DLLs being reachable through PATH
# when launched from native Windows CMake/PowerShell. Add the selected tool directories
# explicitly so gcc can find cc1/as/ld and their runtime DLLs.
$hostToolDirs = @(
    (Split-Path -Parent $compiler)
)
if ($makeTool) {
    $hostToolDirs += (Split-Path -Parent $makeTool)
}
$hostToolDirs = @($hostToolDirs | Select-Object -Unique)
$dirsToPrepend = @($hostToolDirs)
[array]::Reverse($dirsToPrepend)
foreach ($dir in $dirsToPrepend) {
    if ($dir -and (($env:PATH -split ';') -notcontains $dir)) {
        $env:PATH = "$dir;$env:PATH"
    }
}

Write-Host "Checking host C compiler..."
$compilerVersionOutput = & $compiler --version 2>&1
$compilerExitCode = $LASTEXITCODE
$compilerVersionOutput | Select-Object -First 1 | Write-Host
if ($compilerExitCode -ne 0) {
    throw "Host C compiler failed to start with exit code ${compilerExitCode}: $compiler"
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# Clear CMake's toolchain detection cache. This build directory may previously have
# been configured by another CMake/compiler combination.
$cachePathForReset = Join-Path $BuildDir "CMakeCache.txt"
$cmakeFilesForReset = Join-Path $BuildDir "CMakeFiles"
if (Test-Path -LiteralPath $cachePathForReset) {
    Remove-Item -LiteralPath $cachePathForReset -Force
}
if (Test-Path -LiteralPath $cmakeFilesForReset) {
    Remove-Item -LiteralPath $cmakeFilesForReset -Recurse -Force
}

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_C_COMPILER=$compiler"
)
if ($Generator) {
    $configureArgs += @("-G", $Generator)
}
if ($makeTool) {
    $configureArgs += "-DCMAKE_MAKE_PROGRAM=$makeTool"
}

Write-Host "Configuring n3ds-preprocess..."
Write-Host "  CMake: $cmake"
Write-Host "  Source: $sourceDir"
Write-Host "  Build:  $BuildDir"
Write-Host "  Compiler: $compiler"
Write-Host "  Generator: $Generator"
if ($makeTool) {
    Write-Host "  Make tool: $makeTool"
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
$configuredCompiler = $compiler
$cachePath = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path -LiteralPath $cachePath) {
    $compilerLine = Get-Content -LiteralPath $cachePath |
        Where-Object { $_ -match '^CMAKE_C_COMPILER:(FILEPATH|STRING)=' } |
        Select-Object -First 1
    if ($compilerLine) {
        $configuredCompiler = ($compilerLine -split '=', 2)[1]
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
    c_compiler = $configuredCompiler
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
