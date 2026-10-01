# Spec 01 — CMake Build Support and Superquadric Fix

## Requirements

### Problem Statement
- CMake compiles fine with `-DENABLE_SQ=ON`, but runtime fails with "unknown atom style"
- Root cause: stale `style_*.h` files or `SUPERQUADRIC_ACTIVE_FLAG` not reaching compiler
- CI never tests the superquadric path (`.github/workflows/cmake-single-platform.yml` doesn't install Boost or pass `-DENABLE_SQ=ON`)

### Deliverable
- Working CMake build with `-DENABLE_SQ=ON` for the superquadric package
- This is the required reference baseline for comparing the new ss shape against (see spec 03)
- Keep the legacy Makefile/Make.sh path working as primary/fallback build
- This spec adds CMake superquadric support; it does not replace or deprecate the Makefile path

### Success Criteria
- `cmake -B build ./src/ -DENABLE_SQ=ON` configures without errors
- `cmake --build build -j` compiles without errors
- `build/liggghts -in <superquadric_input>` runs without "unknown atom style" error
- Generated `src/style_atom.h` includes `atom_vec_superquadric.h`
- `SUPERQUADRIC_ACTIVE_FLAG` appears in compile commands
