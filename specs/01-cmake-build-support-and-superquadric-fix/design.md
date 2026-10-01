# Spec 01 — CMake Build Support and Superquadric Fix: Design

## Root Cause Analysis

The CMakeLists.txt superquadric logic (lines 475-493):
```cmake
IF(ENABLE_SQ)
  FIND_PACKAGE(Boost)
  IF(Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    ADD_DEFINITIONS(-DSUPERQUADRIC_ACTIVE_FLAG -DNONSPHERICAL_ACTIVE_FLAG)
    TARGET_LINK_LIBRARIES(liggghts_static PUBLIC ${Boost_LIBRARY})
    ...
  ENDIF()
ENDIF()
```

**Root cause**: `ADD_DEFINITIONS` at line 480 is called **after** `ADD_LIBRARY(liggghts_obj OBJECT ${SOURCES})` at line 317. In CMake, `ADD_DEFINITIONS` only affects targets created **after** the call. So `SUPERQUADRIC_ACTIVE_FLAG` never reaches the compiler for `liggghts_obj`. The `#ifdef SUPERQUADRIC_ACTIVE_FLAG` guards in superquadric headers never activate, the `AtomStyle` registration macros never execute, and runtime fails with "unknown atom style".

Additional issues fixed:
1. `find_package(Boost)` → `find_package(Boost REQUIRED)` — fail fast if Boost missing
2. `${Boost_INCLUDE_DIR}` → `${Boost_INCLUDE_DIRS}` (plural) — standard CMake variable
3. `${Boost_LIBRARY}` → `${Boost_LIBRARIES}` (plural) — standard CMake variable

## Fix Applied

Changed `ADD_DEFINITIONS` to `TARGET_COMPILE_DEFINITIONS` on the `liggghts_obj` target:
```cmake
TARGET_COMPILE_DEFINITIONS(liggghts_obj PUBLIC SUPERQUADRIC_ACTIVE_FLAG NONSPHERICAL_ACTIVE_FLAG)
```

This ensures the flag is applied to the object library regardless of call order.

## Verification

- `style_atom.h` includes `atom_vec_superquadric.h` ✓
- `SUPERQUADRIC_ACTIVE_FLAG` in compile flags ✓
- Superquadric test case runs without "unknown atom style" error ✓
