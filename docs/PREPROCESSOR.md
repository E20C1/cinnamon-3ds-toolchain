# 3DS preprocessor reproduction

Milestone 1 establishes a repeatable host-side build and invocation path for `n3ds-preprocess` before any CIA work begins.

No successful local build or game conversion is claimed by this document until the commands below have been run on a development machine and the resulting manifest has been reviewed.

## What the preprocessor does

The inherited `tools/n3ds-preprocess` program reads a GameMaker `data.win` and emits the 3DS-oriented texture and audio layout consumed by Cinnamon. Its command-line interface supports explicit `tex3ds`, texture format, atlas page size, optional sprite replacements, and other conversion controls.

The initial reference workload is UNDERTALE bytecode 16. The command-line path is deliberately used instead of the Windows interactive auto-detection path so that the exact input and output directories are reproducible.

## Prerequisites on Windows

The host machine needs:

- Git, if repository commit recording is desired;
- CMake;
- a host C compiler supported by CMake;
- devkitPro's 3DS tools, including `tex3ds.exe`.

The build script does not install system packages. It first tries tools from `PATH`, then known local locations such as `C:\devkitPro`.

## Build from source

From the repository root in PowerShell:

```powershell
.\scripts\build-preprocessor.ps1
```

The default build directory is:

```text
build/host-n3ds-preprocess
```

The script records a local, Git-ignored manifest at:

```text
build/repro/preprocessor-build.json
```

That manifest includes the repository commit when available, selected CMake generator, detected C compiler, generated executable path, and SHA-256 of the executable.

If CMake cannot select an appropriate Windows host generator automatically, pass one explicitly:

```powershell
.\scripts\build-preprocessor.ps1 -Generator "Ninja"
```

or:

```powershell
.\scripts\build-preprocessor.ps1 -Generator "MinGW Makefiles"
```

## Convert a local game input

Do not copy or commit `data.win` into the repository.

Run:

```powershell
.\scripts\preprocess-game.ps1 -DataWin "C:\path\to\UNDERTALE\data.win"
```

The default output is:

```text
build/game-assets/current
```

This keeps generated copyrighted assets outside tracked source directories.

The wrapper resolves `tex3ds`, invokes the source-built preprocessor with explicit options, and verifies that at least these inherited outputs exist:

```text
gfx/atlas.bin
gfx/direct_assets.bin
audio/sound_bank.bin
```

It then records a local manifest at:

```text
build/repro/last-preprocess.json
```

The manifest records hashes needed to reproduce and compare the conversion without storing the user's game data in Git.

To write somewhere else:

```powershell
.\scripts\preprocess-game.ps1 `
    -DataWin "C:\path\to\game\data.win" `
    -OutputDir "D:\cinnamon-assets"
```

## Historical reference binary

The direct fork parent published a Linux `n3ds-preprocess` binary in its `patch` release with SHA-256:

```text
5acc329304d44b42ab2c3f2dc41ade508770fa5086a263a71276e38292cffea3
```

It remains a behavioral reference, not the canonical build input for this fork. The canonical objective is to reproduce the host tool from the checked-in source.

## Boundary for other games

The wrapper is intentionally named `preprocess-game.ps1`, not `preprocess-undertale.ps1`, because the underlying format work is not inherently limited to one title.

That does not establish compatibility with arbitrary GameMaker games. The inherited preprocessor and runner contain UNDERTALE-oriented conveniences, and runtime compatibility still depends on bytecode version, GML/API coverage, extensions, resource formats, memory use, rendering behavior, and New 3DS hardware limits.
