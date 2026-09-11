# Fable II — Native Recompilation (Experimental)

An experimental static recompilation of Fable II for modern x86-64 Linux,
built with [ReXGlue](https://github.com/Rexicon226/rexglue-sdk).
It recompiles the game's Xbox 360 PowerPC executable into native code while
preserving its observable behavior. This is not a source reconstruction.

**Status: experimental and not broadly playable.** It reaches early
Bowerstone gameplay, with major graphics/performance issues. See
[docs/re/status.md](docs/re/status.md) for the current milestone and blocker.

## You provide the game

This repository contains **no game assets, no executable, and no keys**.

To build and run it you must supply your own legally obtained copy of the
game (a dumped game filesystem containing `default.xex`, e.g. extracted from
a Games-on-Demand package you own). Anything ignored by `.gitignore`
(`assets-extracted/`, `generated/`, `build/`, `logs/`, SDK checkouts) is
local-only and never committed.

## Prerequisites

- x86-64 Linux
- [Nix](https://nixos.org/) (the pinned shell provides Clang, CMake, Ninja,
  and the build/extraction dependencies)
- Your own game copy (see above)
- A checkout of the ReXGlue SDK at the revision recorded in
  `docs/re/sdk-submodules.txt`

## Quickstart

```sh
nix develop
scripts/build-sdk     # build the ReXGlue CLI
scripts/codegen       # analyze default.xex, generate native sources
scripts/build         # configure + compile (BUILD_JOBS controls parallelism)
scripts/run           # bounded headless run (RUN_SECONDS, RUN_STOP_SIGNAL)
```

Game extraction (only needed once per game copy):

```sh
scripts/build-extractor
scripts/extract-game
```

Useful follow-ups: `scripts/inspect-last-run` summarizes the latest run log;
`docs/re/iteration-loop.md` describes the bring-up loop.

## Layout

| Path                   | What it is                                            |
| ---------------------- | ----------------------------------------------------- |
| `src/`                 | Native overrides and runtime integration (yours)      |
| `fable_ii_manifest.toml` | ReXGlue project config: entrypoint + function bounds |
| `scripts/`             | Reproducible extract / codegen / build / run helpers  |
| `docs/re/`             | Reverse-engineering notes, status, function evidence  |
| `generated/`           | ReXGlue output (local-only, never edit by hand)       |

## Contributing notes

- Never hand-edit `generated/` — fix codegen config, overrides, hooks, or
  the SDK instead.
- New function boundaries in `fable_ii_manifest.toml` need evidence in
  `docs/re/functions.md` (callers, byte patterns, neighboring functions).
- One causal change per experiment; record the blocker, evidence, and result
  in `docs/re/status.md`.
