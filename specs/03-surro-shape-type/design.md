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
| Characteristic length | `atom->characteristic_length[i]` (double) | new per-particle array; set by `set ... diameter` (L = D), default 1.0 |
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
- Runtime (Phase 3C): one shape type per simulation, enforced in `AtomVecSurrogate::init()`
- Future (Phase 3D, R03.6a): several shapes per simulation via per-particle `shape_key`;
  see R03.9 for what still has to change

## Characteristic Length Assignment (R03.3, 2026-11-17)

`src/set.cpp`, keyword `DIAMETER` (dual-guarded):
```cpp
atom->radius[i] = 0.5 * dvalue * force->cg(atom->type[i]);
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag)
  atom->characteristic_length[i] = 2.0 * atom->radius[i];
#endif
```
`AtomVecSurrogate::create_atom` / `data_atom` keep the defaults `radius = 0.5`, `L = 1.0`.
`characteristic_length` is already packed in exchange/restart buffers.

## Ghost Communication (2026-11-17)

`pack_border` / `unpack_border` (and `_vel`) also send `radius`, `characteristic_length` and
`shape_key`, so ghost particles carry their shape (needed for future ss-ss contact);
`size_border` matches the packed count.
