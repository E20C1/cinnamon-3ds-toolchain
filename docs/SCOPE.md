# Project scope

This fork is maintained as a reproducible Nintendo 3DS build and packaging toolchain for Cinnamon.

## Current target

The current supported target for this repository is **New Nintendo 3DS / New Nintendo 3DS XL / New Nintendo 2DS XL** hardware.

Old Nintendo 3DS / 3DS XL / 2DS compatibility is intentionally out of scope for the initial milestone. It may be added later, but current implementation and acceptance testing may use New 3DS-specific performance assumptions where doing so materially improves reliability or performance.

This scope statement does not mean the current inherited Cinnamon source already enforces a New 3DS-only runtime. The baseline code is still the upstream 3DS implementation. New 3DS-specific behavior must be introduced explicitly and documented when implemented.

## Initial reference game

The first reference workload is UNDERTALE using GameMaker bytecode version 16.

The repository must not contain copyrighted game data. In particular, `data.win`, generated assets derived from a user's copy of a game, and installable game packages containing those assets must remain local build inputs/outputs unless their redistribution is legally permitted.

## Longer-term direction

The toolchain should remain structured so that other compatible GameMaker games can be evaluated later. Game-specific patches and metadata should be isolated from reusable preprocessing, build, and CIA packaging logic.

Compatibility with another game is not implied solely by matching GameMaker or bytecode versions. Unsupported GML functions, extensions, rendering behavior, audio behavior, resource size, memory pressure, and 3DS hardware limits may require game-specific work.
