# Spec 03 — Surrogate Shape Type: Design

## Atom Class Changes

### atom.h
```cpp
int surrogate_flag;  // new flag, line ~161
```

### atom.cpp
```cpp
surrogate_flag = 0;  // initialize to 0, line ~172
```

### atom_vec_surrogate.h/cpp (new files)
- Follows `atom_vec_superquadric.h/cpp` pattern
- `AtomStyle(surrogate, AtomVecSurrogate)` registration macro
- Sets `atom->surrogate_flag = 1` in constructor
- Allocates per-particle arrays: quaternion, shape_key, characteristic_length

## Per-Particle Data

| Data | Type | Source |
|------|------|--------|
| Position | `atom->x[3]` | existing |
| Orientation | `atom->quaternion[4]` | existing (if using granular atom style) |
| Characteristic length | `double` | new per-particle array |
| Shape key | `int` | new per-particle array |

## Shape Database

```cpp
// Global, follows LIGGGHTS convention
std::vector<std::string> shape_names;  // index = shape key
std::vector<int> shape_keys;           // parallel to names (redundant but fast)

// At init: read shape names from input file, populate vectors
// At runtime: use shape key (int) for fast model lookup
```

## Dual-Guard Pattern

```cpp
#ifdef SURROGATE_ACTIVE_FLAG
  if(atom->surrogate_flag) {
    // surrogate-specific code
  }
#endif
```

## Coexistence

- Compile-time: `ENABLE_SURROGATE` CMake option defines `SURROGATE_ACTIVE_FLAG`
- Runtime: one shape type per simulation (like superquadrics/multisphere)
- No mixing of shape types in same simulation
