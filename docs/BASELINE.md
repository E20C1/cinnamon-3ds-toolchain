# Baseline provenance

This document records the source baseline used before E20C1-specific toolchain work begins.

## Repository lineage

- Working repository: `E20C1/cinnamon-3ds-toolchain`
- Direct fork parent: `KrisTheDog/cinnamon-ignore`
- Parent project: `Project-Sunshine-Native/cinnamon`
- Baseline branch: `main`
- Baseline commit: `6ae19c5577a62ab6d61890d330d8228a3242a6e0`
- Baseline commit subject: `Fix buffer overflow in appendDebugName function`

The direct fork parent was selected because this snapshot contains the 3DS backend, `tools/n3ds-preprocess`, `build-windows-n3ds.ps1`, and the CMake `PLATFORM=n3ds` path needed to reproduce the older public 3DS build.

## Known 3DS baseline behavior

At this baseline:

- `PLATFORM=n3ds` builds the Nintendo 3DS backend using libctru, citro2d, and citro3d.
- The 3DS CMake path forces GameMaker bytecode version 16 support on and bytecode version 17 support off.
- `resources/3ds/romfs` is packaged into the generated `.3dsx` when present.
- The runtime also checks `sdmc:/3ds/cinnamon` for preprocessed assets.
- `tools/n3ds-preprocess` converts GameMaker resources into the 3DS-oriented asset layout expected by the runner.
- No CIA build target is present yet.

These statements describe the inherited baseline only. They do not establish that a fresh build has already been reproduced in this fork.

## Reference preprocessor binary

A historical Linux build of `n3ds-preprocess` is available from the `patch` release of `KrisTheDog/cinnamon-ignore`.

Reference SHA-256:

`5acc329304d44b42ab2c3f2dc41ade508770fa5086a263a71276e38292cffea3`

The binary is a reference artifact only. The long-term reproducibility target is to build `n3ds-preprocess` from source and record the exact host/toolchain versions used for a known-good build.

## Reproducibility rule

A successful milestone should record:

- repository commit SHA;
- devkitPro/devkitARM/libctru versions;
- CMake and host environment;
- preprocessor build provenance;
- input game identity/version without committing copyrighted game data;
- hashes of locally generated 3DSX/CIA outputs where useful.

The first verified build will extend this document with the exact tool versions and output hashes.
