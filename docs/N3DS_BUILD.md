# Reproducible Nintendo 3DS build

Milestone 2 builds Cinnamon for Nintendo 3DS from the checked-in source and, by default, embeds the locally preprocessed GameMaker assets from `build/game-assets/current` into the 3DSX RomFS.

The initial hardware validation target is the **New Nintendo 3DS family**. This build wrapper does not yet add a hard runtime New-3DS-only check; that policy belongs to Milestone 3.

## Prerequisites

Run the Windows bootstrap first:

```text
setup-windows.cmd
```

Then preprocess a user-owned `data.win`:

```powershell
.\scripts\preprocess-game.ps1 -DataWin "C:\path\to\UNDERTALE"
```

The default build requires these generated files:

```text
build/game-assets/current/gfx/atlas.bin
build/game-assets/current/gfx/direct_assets.bin
build/game-assets/current/audio/sound_bank.bin
```

If `build/repro/last-preprocess.json` exists, the 3DS build wrapper verifies those files against the hashes recorded by the preprocessing step before compiling.

## Build with embedded assets

From PowerShell:

```powershell
.\build-windows-n3ds.ps1
```

Or double-click:

```text
build-new3ds.cmd
```

The wrapper uses the devkitPro MSYS2 environment and the current 3DS CMake toolchain under `C:\devkitPro`. Windows paths are converted to MSYS paths before CMake is invoked, so the host-side path conversion problem encountered by the standalone preprocessor build does not apply here.

By default CMake receives:

```text
PLATFORM=n3ds
CMAKE_BUILD_TYPE=Release
N3DS_ROMFS_DIR=<repo>/build/game-assets/current
N3DS_DISABLE_BOTTOM_SCREEN=OFF
```

The generated asset directory itself is used as the RomFS root. This matches the runtime layout:

```text
romfs:/gfx/...
romfs:/audio/...
romfs:/<streamed music files>
```

The runtime also retains its SD-card fallbacks under `sdmc:/3ds/cinnamon`.

## Build without embedded assets

For a runner-only 3DSX that expects assets on the SD card:

```powershell
.\build-windows-n3ds.ps1 -NoRomfs
```

Place the preprocessed asset directory contents under:

```text
sdmc:/3ds/cinnamon/
```

on the 3DS SD card.

## Alternate asset directory

```powershell
.\build-windows-n3ds.ps1 -AssetDir "D:\cinnamon-assets"
```

The directory must have the same root layout as the preprocessor output.

## Outputs and reproducibility manifest

The expected primary output is:

```text
build/n3ds/cinnamon.3dsx
```

The wrapper records:

```text
build/repro/n3ds-build.json
```

The manifest includes the repository commit, configuration, devkitPro tool paths, RomFS asset-tree SHA-256, preprocess-manifest SHA-256 when available, and SHA-256 for the produced `cinnamon.3dsx`. If `cinnamon.elf` is available, its path and hash are recorded too.

The asset-tree hash is computed from sorted relative paths plus the SHA-256 of every file in the selected asset directory. This allows two builds to prove which exact local asset tree was packaged without committing proprietary game data.

## CMake RomFS override

The root CMake project now exposes `N3DS_ROMFS_DIR` as a cache path. Its default remains:

```text
resources/3ds/romfs
```

so the inherited manual workflow remains compatible. The Windows wrapper overrides it with `build/game-assets/current` by default, keeping generated copyrighted assets in the ignored build tree.

## Hardware validation boundary

A successful local build completes the build portion of Milestone 2, but Milestone 2 acceptance still requires launching the resulting `cinnamon.3dsx` on New Nintendo 3DS hardware and reaching playable UNDERTALE behavior.
