[CmdletBinding()]
param(
    [switch]$SkipPreprocessorBuild,
    [switch]$SkipDevkitProInstaller
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Resolve-Executable {
    param(
        [string[]]$CommandNames,
        [string[]]$FallbackPaths
    )

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

function Require-Winget {
    $winget = Resolve-Executable -CommandNames @("winget.exe", "winget") -FallbackPaths @()
    if (-not $winget) {
        throw "winget was not found. Install Microsoft's App Installer, then run setup-windows.cmd again."
    }
    return $winget
}

function Ensure-WingetPackage {
    param(
        [string]$Id,
        [string]$DisplayName,
        [scriptblock]$IsInstalled
    )

    if (& $IsInstalled) {
        Write-Host "[OK] $DisplayName is already installed."
        return
    }

    $winget = Require-Winget
    Write-Step "Installing $DisplayName"
    & $winget install --id $Id -e --accept-package-agreements --accept-source-agreements --silent
    $exitCode = $LASTEXITCODE

    if (& $IsInstalled) {
        Write-Host "[OK] $DisplayName is installed."
        return
    }

    throw "$DisplayName installation did not produce the expected files (winget exit code $exitCode)."
}

function Test-CMake {
    return [bool](Resolve-Executable -CommandNames @("cmake.exe", "cmake") -FallbackPaths @(
        "C:\Program Files\CMake\bin\cmake.exe"
    ))
}

function Test-Git {
    return [bool](Resolve-Executable -CommandNames @("git.exe", "git") -FallbackPaths @(
        "C:\Program Files\Git\cmd\git.exe",
        "C:\Program Files\Git\bin\git.exe"
    ))
}

function Test-StandardMsys2 {
    return Test-Path -LiteralPath "C:\msys64\usr\bin\pacman.exe" -PathType Leaf
}

function Test-HostCompiler {
    return Test-Path -LiteralPath "C:\msys64\ucrt64\bin\gcc.exe" -PathType Leaf
}

function Test-HostNinja {
    return Test-Path -LiteralPath "C:\msys64\ucrt64\bin\ninja.exe" -PathType Leaf
}

function Ensure-HostToolchain {
    if ((Test-HostCompiler) -and (Test-HostNinja)) {
        Write-Host "[OK] MSYS2 UCRT64 GCC and Ninja are already installed."
        return
    }

    $pacman = "C:\msys64\usr\bin\pacman.exe"
    if (-not (Test-Path -LiteralPath $pacman -PathType Leaf)) {
        throw "Standard MSYS2 pacman was not found at $pacman."
    }

    Write-Step "Installing MSYS2 UCRT64 GCC and Ninja"
    $packages = @(
        "mingw-w64-ucrt-x86_64-gcc",
        "mingw-w64-ucrt-x86_64-ninja"
    )

    & $pacman -S --needed --noconfirm @packages
    $firstExit = $LASTEXITCODE

    if ((Test-HostCompiler) -and (Test-HostNinja)) {
        if ($firstExit -ne 0) {
            Write-Warning "pacman returned $firstExit, but the required GCC/Ninja binaries are present. Continuing."
        } else {
            Write-Host "[OK] MSYS2 UCRT64 GCC and Ninja are installed."
        }
        return
    }

    Write-Warning "Initial package install did not leave both required binaries. Refreshing/upgrading MSYS2 package state and retrying."
    & $pacman -Syu --noconfirm
    & $pacman -S --needed --noconfirm @packages

    if (-not ((Test-HostCompiler) -and (Test-HostNinja))) {
        throw "MSYS2 host GCC/Ninja installation failed. Check mirror/network errors and rerun setup-windows.cmd."
    }

    Write-Host "[OK] MSYS2 UCRT64 GCC and Ninja are installed."
}

function Get-DevkitProRequiredPaths {
    return @(
        "C:\devkitPro\tools\bin\tex3ds.exe",
        "C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe",
        "C:\devkitPro\libctru",
        "C:\devkitPro\cmake\3DS.cmake",
        "C:\devkitPro\libctru\lib\libcitro2d.a",
        "C:\devkitPro\libctru\lib\libcitro3d.a"
    )
}

function Get-MissingDevkitProPaths {
    $missing = @()
    foreach ($path in (Get-DevkitProRequiredPaths)) {
        if (-not (Test-Path -LiteralPath $path)) {
            $missing += $path
        }
    }
    return $missing
}

function Install-DevkitPro3dsGroup {
    $pacman = "C:\devkitPro\msys2\usr\bin\pacman.exe"
    if (-not (Test-Path -LiteralPath $pacman -PathType Leaf)) {
        return $false
    }

    Write-Step "Installing/updating the devkitPro 3DS development group"
    & $pacman -S --needed --noconfirm 3ds-dev
    if ((Get-MissingDevkitProPaths).Count -eq 0) {
        Write-Host "[OK] devkitPro 3DS development components are installed."
        return $true
    }

    return $false
}

function Launch-DevkitProInstaller {
    if ($SkipDevkitProInstaller) {
        return
    }

    Write-Step "Downloading the official devkitPro Windows installer"
    Write-Host "The installer UI will open. Select '3DS Development' and complete the installation."
    Write-Host "This bootstrap will wait for it to close, then verify the required files."

    $headers = @{
        "User-Agent" = "cinnamon-3ds-toolchain-bootstrap"
        "Accept" = "application/vnd.github+json"
    }
    $release = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/devkitPro/installer/releases/latest"
    $asset = $release.assets |
        Where-Object { $_.name -match '\.exe$' } |
        Select-Object -First 1

    if (-not $asset) {
        throw "Could not locate an .exe asset in the latest devkitPro installer release."
    }

    $downloadDir = Join-Path $env:TEMP "cinnamon-3ds-toolchain"
    New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
    $installerPath = Join-Path $downloadDir $asset.name

    Invoke-WebRequest -Headers $headers -Uri $asset.browser_download_url -OutFile $installerPath
    Write-Host "Downloaded: $installerPath"
    Write-Host "SHA-256: $((Get-FileHash -Algorithm SHA256 -LiteralPath $installerPath).Hash.ToLowerInvariant())"

    $process = Start-Process -FilePath $installerPath -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        Write-Warning "devkitPro installer exited with code $($process.ExitCode). Verification will decide whether setup can continue."
    }
}

function Ensure-DevkitPro3ds {
    $missing = Get-MissingDevkitProPaths
    if ($missing.Count -eq 0) {
        Write-Host "[OK] devkitPro 3DS development components are already installed."
        return
    }

    if (Test-Path -LiteralPath "C:\devkitPro\msys2\usr\bin\pacman.exe" -PathType Leaf) {
        if (Install-DevkitPro3dsGroup) {
            return
        }
    } else {
        Launch-DevkitProInstaller

        if (Test-Path -LiteralPath "C:\devkitPro\msys2\usr\bin\pacman.exe" -PathType Leaf) {
            if (Install-DevkitPro3dsGroup) {
                return
            }
        }
    }

    $missing = Get-MissingDevkitProPaths
    if ($missing.Count -gt 0) {
        $detail = $missing -join [Environment]::NewLine
        throw ("devkitPro 3DS setup is incomplete. Missing paths:" + [Environment]::NewLine + $detail)
    }
}

function Write-EnvironmentSummary {
    Write-Step "Environment summary"

    $checks = [ordered]@{
        "CMake" = (Test-CMake)
        "Git" = (Test-Git)
        "MSYS2 pacman" = (Test-StandardMsys2)
        "Host GCC" = (Test-HostCompiler)
        "Host Ninja" = (Test-HostNinja)
        "tex3ds" = (Test-Path -LiteralPath "C:\devkitPro\tools\bin\tex3ds.exe")
        "devkitARM" = (Test-Path -LiteralPath "C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe")
        "libctru" = (Test-Path -LiteralPath "C:\devkitPro\libctru")
        "3DS.cmake" = (Test-Path -LiteralPath "C:\devkitPro\cmake\3DS.cmake")
        "citro2d" = (Test-Path -LiteralPath "C:\devkitPro\libctru\lib\libcitro2d.a")
        "citro3d" = (Test-Path -LiteralPath "C:\devkitPro\libctru\lib\libcitro3d.a")
    }

    foreach ($entry in $checks.GetEnumerator()) {
        $status = if ($entry.Value) { "OK" } else { "MISSING" }
        Write-Host ("{0,-14} {1}" -f $entry.Key, $status)
    }

    $missing = $checks.GetEnumerator() | Where-Object { -not $_.Value }
    if ($missing) {
        throw "Environment verification failed."
    }
}

Write-Host "Cinnamon 3DS Toolchain - Windows environment bootstrap" -ForegroundColor Green
Write-Host "Repository: $repoRoot"
Write-Host "Existing components are left in place; missing components are installed."

Ensure-WingetPackage -Id "Git.Git" -DisplayName "Git" -IsInstalled { Test-Git }
Ensure-WingetPackage -Id "Kitware.CMake" -DisplayName "CMake" -IsInstalled { Test-CMake }
Ensure-WingetPackage -Id "MSYS2.MSYS2" -DisplayName "MSYS2" -IsInstalled { Test-StandardMsys2 }

Ensure-HostToolchain
Ensure-DevkitPro3ds
Write-EnvironmentSummary

if (-not $SkipPreprocessorBuild) {
    Write-Step "Building n3ds-preprocess from repository source"
    $buildScript = Join-Path $PSScriptRoot "build-preprocessor.ps1"
    & $buildScript
    if ($LASTEXITCODE -ne 0) {
        throw "build-preprocessor.ps1 failed with exit code $LASTEXITCODE."
    }
}

Write-Host ""
Write-Host "Windows environment setup is complete." -ForegroundColor Green
Write-Host "Next step: run scripts\preprocess-game.ps1 with your local data.win path."
