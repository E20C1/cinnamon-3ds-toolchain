# Build plan

The work is intentionally staged so failures can be attributed to one layer at a time.

## Milestone 1: reproduce the inherited preprocessor

Status: **local acceptance reached**. The Windows bootstrap, source-built host preprocessor, local `data.win` conversion, required output validation, and reproducibility manifests have all completed successfully on the reference development machine.

Acceptance: a user-owned UNDERTALE `data.win` can be converted without corruption into the asset layout expected by the 3DS runtime.

## Milestone 2: reproduce the inherited 3DSX build

Status: **acceptance reached on New Nintendo 3DS hardware**. The Release `cinnamon.3dsx` launches from the Homebrew Launcher, reads the user's local `data.win`, reaches playable UNDERTALE behavior, accepts the Japanese language setting, and produces working game audio. The first hardware pass also exposed a text-readability issue: dense Japanese glyphs become visibly degraded after the 3DS top-screen downscale.

Build Cinnamon with `PLATFORM=n3ds` using the generated assets either embedded in RomFS or loaded from `sdmc:/3ds/cinnamon`.

Acceptance: the resulting `cinnamon.3dsx` launches and reaches playable UNDERTALE behavior on New Nintendo 3DS hardware.

## Milestone 3: define New 3DS-specific runtime policy

Measure whether New 3DS-specific CPU/cache features or resource assumptions are required or beneficial. Any such requirement must be explicit rather than accidental.

Acceptance: the supported hardware requirement is documented and the runtime fails clearly if a hard New 3DS requirement is introduced.

## Milestone 4: add CIA packaging

Add a reproducible CIA target from the native build outputs rather than treating a post-hoc 3DSX-to-CIA conversion as the primary path.

Acceptance: an installable CIA launches from the HOME Menu on New Nintendo 3DS and uses the same validated runner/assets as the working 3DSX build.

## Milestone 5: reduce manual conversion work

Wrap preprocessing and packaging in scripts that accept a local game input path and produce deterministic build directories while preventing copyrighted input data from being committed.

## Milestone 6: generalize for other GameMaker games

Separate reusable toolchain logic from game-specific metadata, overrides, and compatibility patches.

Acceptance for each additional game is explicit and evidence-based; matching bytecode version alone is not treated as sufficient compatibility.
