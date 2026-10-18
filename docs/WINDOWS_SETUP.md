# Windows environment setup

This repository provides a bootstrap for a fresh Windows development machine. The goal is to reduce the environment setup used by the New 3DS workflow to one entry point while keeping every dependency check explicit.

## One-click setup

From a Git clone of this repository, double-click:

```text
setup-windows.cmd
```

The launcher invokes:

```text
scripts/setup-windows.ps1
```

Existing components are detected and skipped. Missing components are installed or repaired, then the bootstrap verifies the complete environment and builds `n3ds-preprocess` from repository source.

The bootstrap currently manages:

- Git;
- CMake;
- standard MSYS2 under `C:\msys64`;
- UCRT64 host GCC and Ninja for building Windows host tools;
- devkitPro 3DS development components under `C:\devkitPro`;
- `tex3ds`;
- devkitARM;
- libctru;
- citro2d;
- citro3d;
- the devkitPro `3DS.cmake` toolchain file.

## Why there are two MSYS2 installations

They serve different roles:

```text
C:\msys64
  Windows host toolchain
  -> gcc.exe
  -> ninja.exe
  -> builds n3ds-preprocess.exe

C:\devkitPro
  Nintendo 3DS toolchain
  -> devkitARM
  -> libctru / citro2d / citro3d
  -> tex3ds
  -> 3DS.cmake
  -> later builds cinnamon.3dsx / CIA-related outputs
```

The standard MSYS2 package repository is intentionally not assumed to exist inside devkitPro's bundled MSYS2.

## devkitPro on a completely fresh PC

If `C:\devkitPro` does not exist, the bootstrap downloads the latest Windows installer from the official `devkitPro/installer` GitHub release and starts it.

The devkitPro installer is interactive. Select **3DS Development** and complete the installer. The bootstrap waits for it to close and then verifies the required 3DS files. If devkitPro itself is already present but 3DS components are missing, the bootstrap tries the official `3ds-dev` package group through devkitPro's pacman first.

The upstream installer source itself defines a **3DS Development** section and installs the `3ds-dev` group for that selection.

## Package mirror errors

MSYS2 can occasionally print a transient mirror error such as an HTTP 404 while another mirror still supplies the package. For that reason, this bootstrap verifies the actual required binaries after a package command instead of treating one mirror message as proof of failure.

For example, if both of these exist after installation:

```text
C:\msys64\ucrt64\bin\gcc.exe
C:\msys64\ucrt64\bin\ninja.exe
```

the host compiler step is considered successful.

If the files are still missing, the bootstrap refreshes/upgrades the MSYS2 package state once and retries before stopping with an error.

## PowerShell-only usage

The same bootstrap can be run directly:

```powershell
.\scripts\setup-windows.ps1
```

To install/verify dependencies without building the preprocessor at the end:

```powershell
.\scripts\setup-windows.ps1 -SkipPreprocessorBuild
```

To prevent the script from opening the devkitPro graphical installer automatically:

```powershell
.\scripts\setup-windows.ps1 -SkipDevkitProInstaller
```

## What the bootstrap does not install

Game files are never downloaded or copied by the bootstrap. `data.win` remains a user-supplied local input and must not be committed to this repository.

After setup succeeds, continue with:

```powershell
.\scripts\preprocess-game.ps1 -DataWin "C:\path\to\UNDERTALE\data.win"
```

## citro2d / citro3d install location

Current devkitPro packages install citro2d and citro3d into the libctru prefix. The bootstrap therefore verifies:

```text
C:\devkitPro\libctru\lib\libcitro2d.a
C:\devkitPro\libctru\lib\libcitro3d.a
```

They are not expected under `C:\devkitPro\portlibs\3ds\lib` in this toolchain layout.
