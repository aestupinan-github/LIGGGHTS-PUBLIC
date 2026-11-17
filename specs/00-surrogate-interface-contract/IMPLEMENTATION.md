# Spec 00 Implementation Reference

**Status:** Implemented and verified  
**Date:** 2026-11-10  
**Target:** Surrogate Shape Interface Contract (Spec 00)

> **Revised 2026-11-17.** The earlier log10/eps formulas and the z range [-0.5, 2.0]
> documented here were wrong for run_029 and have been removed from the code. See
> requirements.md (revision note, R00.1–R00.5) for the verified contract.

## Summary

Three headers implement the Spec 00 interface contract:

1. **`src/surrogate_interface.h`** — low-level math, no LAMMPS dependencies
   - `h_exact_cube(q, dir_wall)` — exact support height of the unit cube toward the wall
   - `denormalize_signed_distance(raw, y_mean, y_scale, h)` — `raw*y_scale + y_mean - h`
   - `rescale_to_physical(value_unit, L_actual)`
   - quaternion canonicalization / normalization, input-tensor preparation

2. **`src/surrogate_validator.h`** — wraps the above with `error->one(FLERR, ...)`

3. **`src/surrogate_contact.h`** — model loading (option4 only) and `query_overlap(...)`

## Key Formulas

```cpp
// input (float32)
z_unit = centre_distance / L_actual;               // fix_wall_gran: centre_distance = deltan + radius
// output
double h       = h_exact_cube(q_canonical, dir_wall);
double sd_unit = (raw * y_scale + y_mean) - h;     // run_029: y_mean 0.7425085086, y_scale 0.1314225569
double sd_phys = sd_unit * L_actual;               // > 0 gap, <= 0 contact
```

### Quaternion Canonicalization (R00.6)

```cpp
if (quat[0] < 0.0) { quat[0] = -quat[0]; quat[1] = -quat[1]; quat[2] = -quat[2]; quat[3] = -quat[3]; }
```

## Usage (Spec 04, fix_wall_gran)

```cpp
double sd_unit;
double sd_phys = surrogate_contact_model_->query_overlap(
  z_unit, L_actual, atom->quaternion[i], dir_wall, atom->tag[i], update->ntimestep, sd_unit);
deltan = sd_phys;
```

## Constants and thresholds

From scalers.dat (not hard-coded): `y_mean`, `y_scale`, `z_prefilter` (1.0), `sd_too_deep` (-0.1).
Hard-coded in `surrogate_interface.h`: `CUBE_HALF_EDGE_UNIT = 0.5`.

## Error Messages (Spec 04, R04.3)

All errors prefixed with particle ID and context. Examples:

```
ERROR: Invalid characteristic_length at particle 42: L_actual=-0.5 (must be > 0 and finite)
ERROR: Invalid z_unit (NaN/Inf) at particle 17
ERROR: Invalid quaternion (contains NaN/Inf) at particle 8: [nan, 0.1, 0.2, 0.3]
ERROR: Surrogate inference produced NaN/Inf at particle 99 with z_unit=1.5
ERROR: Overflow/underflow in denormalization at particle 5: overlap_unit=nan   (sd_unit)
ERROR: Invalid physical overlap at particle 33: overlap_phys=inf
```

## Dual-Guard Pattern (Spec 04, R04.1)

All surrogate code must be wrapped with:

```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag) {
    // Surrogate contact calculation here
    double overlap_phys = ...;
}
#endif
```

This pattern is enforced in all contact calculations (pair potentials, fixes).

## Verification

- C++ path (LibTorch float32 + h_exact_cube) reproduces the Python reference to 1e-6.
- Simulations: L=1 m and L=0.01 m cube drops give the same dimensionless response
  (restitution 0.90, penetration / L consistent with Hertz). See Spec 04.
