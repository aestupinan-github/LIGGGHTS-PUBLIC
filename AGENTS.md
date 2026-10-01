# AGENTS.md

LIGGGHTS-PUBLIC 3.8.0 — open-source DEM (Discrete Element Method) particle simulation in C++, derived from LAMMPS (23 Nov 2013). GPL-2.0.

## Build

Two build systems exist. **CMake is the one CI uses** (`.github/workflows/cmake-single-platform.yml`); the `src/MAKE/` makefiles are the legacy alternative.

```bash
cmake -B build ./src/ -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# binary: build/liggghts
```

- CMake configure **requires MPI and VTK by default** (`ENABLE_MPI`, `ENABLE_VTK` default ON; configure fails with FATAL_ERROR if not found). Minimal serial build: add `-DENABLE_MPI=OFF -DENABLE_VTK=OFF`.
- Superquadrics need Boost: `-DENABLE_SQ=ON`.
- Legacy make build (auto-detects MPI/VTK): `cd src && make auto` (first run generates `MAKE/Makefile.user` from `Makefile.user_default`); `make lib` / `make shlib` for the shared library.

## Architecture

- Entrypoint: `src/main.cpp` — MPI_Init, construct `LAMMPS`, run input file. Everything else follows the LAMMPS style-class design: commands/fixes/computes/dumps/pairs self-register via macros (`FIX_CLASS`, `PAIR_CLASS`, ...) in headers.
- **Style discovery happens at CMake configure time**: `SCAN_STYLES()` in `src/cMake/Style.cmake` globs headers (e.g. `fix_*.h`, `pair_*.h`) and reads them for the class macros. After adding or renaming a style header you **must re-run cmake configure** (or wipe `build/`) or the new style will not compile in. The generated `src/style_*.h` files are untracked by git — never edit or commit them.
- **Contact models are compile-time selected**, not runtime: `ENABLE_MODEL_*` options in `src/CMakeLists.txt` pick normal/tangential/cohesion/rolling/surface models; configure generates `src/style_contact_model.h` with the cross-product of `GRAN_MODEL(...)` combinations. Under the legacy make build, regenerate with `sh Make.sh models` (combinations capped at 1200; extend via `style_contact_model_user.whitelist`).
- `src/cMake/` holds the custom CMake macros (Model/Style/Version/Macros) — changes to build logic go there, not just `CMakeLists.txt`.
- `python/liggghts.py` is a ctypes wrapper over `libliggghts.so`; it only works when the shared library was built (CMake target `liggghts_shared`, or `make shlib`). Use `python/install.py` to copy the library and wrapper to system dirs.

## Testing

There is **no test suite** — CI's `ctest` step passes vacuously (no `add_test` anywhere). Verification = build, then run an example input, e.g.:

```bash
build/liggghts -in examples/LIGGGHTS/Tutorials_public/cohesion/in.cohesion
```

## Repo layout

- `src/` — all source (~850 files, flat). `Cases/` is a scratch area for simulation runs/results (incl. ML overlap-model experiments), not library code — don't treat it as source.
- `examples/LIGGGHTS/Tutorials_public/` — canonical input scripts (`in.*`) per feature.
- `doc/` — HTML/txt manual. `lib/` — vendored LAMMPS package libs (gpu, cuda, meam, ...), not needed for the core build.

## Conventions

- `opencode.json` disables git integration — don't commit unless asked.
- C++14/17 (GCC >= 7 gets C++17, older gets C++14), `-O2 -ffast-math` set by the CMake build; don't add flags that contradict the per-compiler defaults in `src/CMakeLists.txt`.

## Project: Surrogate Shape (ss)

**Purpose**: Add a new particle shape family — "surrogate" (`atom_style surrogate`) — whose wall/particle overlap is predicted by a trained ML surrogate (LibTorch), analogous to how superquadrics work. The surrogate replaces the **geometric** contact calculation (overlap, contact point, normal); physics models (Hertz, Hooke, etc.) remain unchanged.

**Non-negotiable rules**: See `CONSTITUTION.md` — automatically interpreted and enforced at session start.
