# MEMORY.md — Surrogate Shape Project

Persistent memory for the surrogate shape (ss) project. Updated at the end of every working session.

---

## Confirmed Facts

### Build System
- CMake is the CI build system; legacy `src/MAKE/` makefiles are the fallback.
- CMake requires MPI and VTK by default; minimal serial build: `-DENABLE_MPI=OFF -DENABLE_VTK=ON`.
- Superquadrics require Boost and `-DENABLE_SQ=ON`.
- Style discovery (`SCAN_STYLES()`) happens at CMake configure time — adding/renaming style headers requires reconfigure or wiping `build/`.
- Generated `style_*.h` / `style_contact_model.h` are build artifacts — never edit or commit them.
- Contact models are compile-time selected via `ENABLE_MODEL_*` CMake options.

### Superquadric Implementation (reference for ss)
- All superquadric files are flat in `src/` (no `src/SUPERQUADRIC/` subdirectory).
- `atom->superquadric_flag` declared in `atom.h:161`, initialized to 0 in `atom.cpp:172`, set to 1 in `atom_vec_superquadric.cpp:81`.
- Per-atom arrays: `shape` (3 half-axes), `blockiness` (2 params), `quaternion` (4), `angmom` (3), `inertia` (3), `volume`, `area`.
- **Dual-guard pattern**: `#ifdef SUPERQUADRIC_ACTIVE_FLAG` + `if(atom->superquadric_flag)` used together throughout. The `#ifdef` alone segfaults on non-superquadric atom styles.
- Contact history hard-zeroing when `deltan > 0.0` (fix_wall_gran.cpp:1395-1401).
- Closest-point search for primitive walls via `Superquadric::plane_intersection()`.
- Mesh-contact via `tri_mesh_I_superquadric.h` → `resolveTriSuperquadricContact()`.
- ss-vs-ss contact via `surface_model_superquadric.h` (SurfaceModel architecture, not a separate pair style).
- Two atom styles map to same class: `superquadric` and `granular_superquadric` → `AtomVecSuperquadric`.

### LibTorch / ML Current State (discarded ML_implementation branch)
- CMake integration: entirely commented out (src/CMakeLists.txt:505-511).
- Makefile.auto: hardcoded to user's home directory — not portable.
- The surrogate result was computed but never used (`//deltan = deltan_surro;` commented out).
- Scaler format mismatch: code parses 1 value per line, `scalers.dat` has 4 values per line.
- Model loaded from relative paths (`overlap_model.pt`, `scalers.dat`).
- Input features: `[z_unit, qw, qx, qy, qz]` (5 features).
- Output: single scalar `log10(overlap)`.

### SQ Build Issue (RESOLVED)
- CMake compiles fine with `-DENABLE_SQ=ON`, but runtime failed with "unknown atom style".
- **Root cause**: `ADD_DEFINITIONS(-DSUPERQUADRIC_ACTIVE_FLAG)` at line 480 was called AFTER `ADD_LIBRARY(liggghts_obj ...)` at line 317. In CMake, `ADD_DEFINITIONS` only affects targets created after the call — so the flag never reached the compiler.
- **Fix**: Changed to `TARGET_COMPILE_DEFINITIONS(liggghts_obj PUBLIC SUPERQUADRIC_ACTIVE_FLAG NONSPHERICAL_ACTIVE_FLAG)`. Also fixed `find_package(Boost REQUIRED)`, `${Boost_INCLUDE_DIRS}` (plural), `${Boost_LIBRARIES}` (plural).
- **Verified**: `style_atom.h` includes superquadric headers, `SUPERQUADRIC_ACTIVE_FLAG` in compile flags, superquadric test case runs without error.

---

## Open Questions

- Scaler format: single-output (1 value) or multi-output (4 values)? Model not frozen yet — revisit when frozen.
- Training data coverage: sufficient for L-rescaling across all particle sizes? Unknown, hoped sufficient for typical DEM magnitudes.
- Multi-material support (Al vs Regolith): future work at physics model level.
- Mesh-wall contact: future work.
- ss-ss contact: exploratory, future work.

---

## Decisions Made

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Atom style name | `atom_style surrogate` (one style) | Descriptive, follows LIGGGHTS conventions; contact physics selected by pair_style |
| Model folder | `SS_models/<shape>-<object>/overlap_model.pt` + `scalers.dat` | Flat, easy to parse, interaction type clear from folder name |
| Shape identifier | `<int key, string name>` — parallel vectors | Fast access (key) + flexibility (name); vectors preferred over maps for speed |
| Shape database | Global | Follows LIGGGHTS convention (style registrations are global) |
| Shape geometry | Baked into model at training time | More robust than loading STL at runtime; wrapper generates points for postprocessing |
| Model loading | At `init()` | Avoid per-contact overhead |
| Model selection | Input file → shape name → model lookup → int key → runtime key access | Flexible at init, fast at runtime |
| Mesh walls | Future work | Separate shape type for now |
| Scaler format | Single-output (1 value/line) | From previous test; revisit when model frozen |
| Dual-guard | `#ifdef SURROGATE_ACTIVE_FLAG` + `if(atom->surrogate_flag)` | Prevents segfaults on non-surrogate atom styles |
| Doxygen | Add `Doxyfile`; document only files we touch | Follows existing `@brief`/`@param`/`@return` convention |
| Geometric vs. physics | Surrogate = geometric; physics (Hertz, etc.) unchanged | Clean separation of concerns |
| Pair style | Explicit selection + cutoff control | Follows LIGGGHTS convention |
| Multi-material | Future work at physics model level | Shape database is global; materials handled by physics model |

---

## Current Status

- **Phase**: Investigation / spec-writing — COMPLETE
- **Branch**: `surrogate-shape-dev` (cut from `master`)
- **Started**: 2026-10-01
- **Constitution**: `CONSTITUTION.md` created — non-negotiable rules automatically interpreted at session start
- **Next steps**: Review specs, then move to implementation (Phase 4) when ready
