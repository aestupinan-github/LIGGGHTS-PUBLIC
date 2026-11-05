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

## Phase 3C Specification Review and Resolution (2026-11-05)

**Status:** ✅ **SPECIFICATIONS RESOLVED** — All 21 QA issues have been addressed

**Resolution Documents:**
- Full QA review with all 21 issues: `QA_REVIEW_PHASE3C_SPECS_20261105.md`
- Executive summary for quick reference: `QA_REVIEW_EXECUTIVE_SUMMARY.md`
- Navigation and search guide: `QA_REVIEW_INDEX.md`

**Specs Updated:**
- ✅ **Spec 00** (Interface Contract): Scaler format locked, denormalization formula clarified, model identity validation added
- ✅ **Spec 01** (CMake): Verified complete (Superquadric fix in place)
- ✅ **Spec 02** (LibTorch): Path resolution algorithms specified, deployment workflows documented
- ✅ **Spec 03** (Shape Type): Input syntax specified (`atom_style surrogate ss-cube`), initialization detailed
- ✅ **Spec 04** (Contact Models): Physics/geometry boundary enforced, validation checks specified, error handling defined
- ✅ **Spec 05** (Testing): Tolerance adjusted to match model accuracy (1e-4), surrogate-specific tests (RF-15 to RF-20) added

**Critical Issues Resolved:**
1. ✅ Scaler format mismatch → Format frozen to ss_model_record_v1 (actual deliverable format)
2. ✅ Denormalization formula ambiguous → Formula explicit: `10^(log10_output) - eps_small` with C++ pseudocode
3. ✅ Model specificity not validated → Runtime validation added (Spec 00 R00.8, Spec 04 R04.5)
4. ✅ Contact physics boundary not enforced → Specification added (Spec 04 R04.1, hard rule with code pattern example)

**High-Priority Issues Resolved:**
- ✅ Scaler "frozen" state → Format locked; no ambiguity
- ✅ Input validation → Ranges defined, error cases specified (Spec 00, Spec 04)
- ✅ Path resolution → Fallback algorithm documented (Spec 02 design)
- ✅ Shape database initialization → Input syntax defined: `atom_style surrogate ss-cube` (Spec 03 R03.2)
- ✅ Test tolerance → Changed from 1e-6 to 1e-4 (matches model MAE = 6.1e-5)

**Cross-Project Impact:**
- SS_models_training Phase 3B deliverables (run_029, run_030) are now properly referenced and integrated
- Scaler format match confirmed (extended key-value format documented as canonical)
- Denormalization formula linked to training code
- Integration ready: LIGGGHTS team can proceed to implementation phase with full clarity

**Next Phase (3D) Tasks:**
- Implement Spec 00-05 following updated requirements
- Add surrogate-specific unit and integration tests (Spec 05 RF-15 to RF-20)
- Validate end-to-end contact pipeline
- Performance testing and optimization

**Documentation Delivered:**
- Updated requirements.md for Specs 00, 02, 03, 04, 05 with complete details
- Updated design.md for Specs 00, 02 with explicit algorithms and pseudocode
- Updated tasks.md for Specs 00, 02, 03 with implementation checklist
- All 21 QA issues documented and traced back to fixes

---

## Phase 3C Specification Review (2026-11-05)

**Status:** ❌ NOT READY FOR IMPLEMENTATION — 21 issues identified

**QA Review Document:** `/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/QA_REVIEW_PHASE3C_SPECS_20261105.md`

**Critical Issues (Block Implementation):**
1. Scaler format mismatch — Spec 00 prescribes format; actual deliverable uses different format
2. Output denormalization formula ambiguous — operator precedence not specified
3. Model specificity not validated — no runtime check that input shape matches training shape
4. Contact physics boundary not enforced — risk of mixing geometric/physics sources

**Structural Issues (Require Redesign):**
5. Scaler "frozen" state has no objective criteria
6. Input feature validation (ranges, edge cases) missing
7. Model path resolution (abs/rel, env vars) undefined
8. Shape database initialization syntax not specified
9. Test tolerance (1e-6) is 16× tighter than model accuracy (6.1e-5)

**Missing Specifications:**
10–15. Contact point/normal format, model versioning, error recovery, units/coordinates, multi-interaction support, surrogate test coverage

**Cross-Project Issues:**
- See `/home/su_estupinan@private.list.lu/Programs/SS_models_training/PHASE_3C_INTEGRATION_ISSUES_20261105.md` for how SS_models_training Phase 3B deliverables interact with these issues

**Next Steps:** Resolve issues per sign-off checklist in QA review document before proceeding to implementation.

---

## Current Status

- **Phase**: Investigation / spec-writing — COMPLETE; specs drafted but **QA review identifies gaps**
- **Branch**: `surrogate-shape-dev` (cut from `master`)
- **Started**: 2026-10-01
- **Constitution**: `CONSTITUTION.md` created — non-negotiable rules automatically interpreted at session start
- **QA Review**: 2026-11-05 — 21 specification issues identified; documented and cross-linked to training repo
- **Next steps**: Resolve QA issues, update specs 00–05, then move to implementation when ready
