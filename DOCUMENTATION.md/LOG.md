# LOG.md — Project Bitácora

Session-by-session log of the surrogate shape (ss) project. Comprehensive records of discussions appended only when requested.

---

## Session 2026-10-01 — Investigation & Scaffolding

### Discussion Summary

**Project scope**: Add a new particle shape family "surrogate" (`atom_style surrogate`) whose wall/particle overlap is predicted by a trained ML surrogate (LibTorch), analogous to superquadrics. The surrogate replaces the geometric contact calculation (overlap, contact point, normal); physics models (Hertz, Hooke, etc.) remain unchanged.

**Key discussions**:

1. **SQ build issue**: CMake compiles fine with `-DENABLE_SQ=ON` but runtime fails with "unknown atom style". Root cause: stale `style_*.h` files or `SUPERQUADRIC_ACTIVE_FLAG` not reaching compiler. Fix: wipe `build/` and reconfigure from scratch.

2. **Surrogate design**: 
   - Input features: `[z_unit, qw, qx, qy, qz]` (from discarded code)
   - Output: overlap (scalar now; extendable to contact point + normal later)
   - Nondimensionalization: train at L=1, rescale by particle's characteristic length
   - Quaternion canonicalization: qw >= 0
   - Hard constraint: overlap + contact point + normal must come from same geometric evaluation (mixing sources caused energy-gain bugs in prior work)

3. **Shape representation**: Free-form family (cubes now, STL in future). Shape geometry baked into model at training time. Wrapper generates points for postprocessing.

4. **Shape identifier**: `<int key, string name>` — parallel vectors (not maps) for speed. Global database (follows LIGGGHTS convention).

5. **Model storage**: `SS_models/<shape>-<object>/overlap_model.pt` + `scalers.dat`. Model loading at `init()`.

6. **Contact models**: Geometric (surrogate) vs. physics (Hertz, etc.) are separate concerns. ss-wall now, ss-ss and mesh walls future work.

7. **Pair style**: Explicit `pair_style` selection + cutoff control (LIGGGHTS convention). Runtime warning if surrogate particles present but no surrogate pair style active.

8. **Doxygen**: All new code must be Doxygen documented. Modifications to existing code must update/add Doxygen descriptions. Add `Doxyfile`.

### Decisions Taken

- Branch `surrogate-shape-dev` cut from `master` (not ML_implementation)
- One atom style: `atom_style surrogate`
- Model folder: `SS_models/<shape>-<object>/overlap_model.pt` + `scalers.dat`
- Shape identifier: parallel vectors (int key + string name), global database
- Shape geometry baked into model at training; wrapper for postprocessing
- Model loading at `init()`
- Scaler format: single-output (1 value/line); revisit when model frozen
- Dual-guard: `#ifdef SURROGATE_ACTIVE_FLAG` + `if(atom->surrogate_flag)`
- Doxygen: add `Doxyfile`, document only files we touch
- Geometric vs. physics: surrogate = geometric; physics unchanged
- Pair style: explicit selection + cutoff control

### Implemented Today

- Created branch `surrogate-shape-dev` from `master`
- Extended `AGENTS.md` with project purpose, branch policy, working style, Doxygen requirement
- Created `MEMORY.md` (this file's companion)
- Created `LOG.md` (this file)
- Created `specs/` directory with 5 subfolders (00–04)
- Added `Doxyfile`
- **Fixed SQ CMake build**: Root cause was `ADD_DEFINITIONS` called after target creation. Changed to `TARGET_COMPILE_DEFINITIONS`. Verified with superquadric test case.
- Created `CONSTITUTION.md` with non-negotiable rules (automatically interpreted at session start)
- Updated `AGENTS.md` to point to `CONSTITUTION.md` for non-negotiable rules

### Open Questions / Carry-over

- Scaler format: single-output or multi-output? (model not frozen yet)
- Training data coverage for L-rescaling: sufficient? (unknown, hoped sufficient)
- Multi-material support: future work at physics model level
- Mesh-wall contact: future work
- ss-ss contact: exploratory, future work
