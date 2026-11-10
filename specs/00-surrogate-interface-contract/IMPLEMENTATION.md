# Spec 00 Implementation Reference

**Status:** Implemented and verified  
**Date:** 2026-11-10  
**Target:** Surrogate Shape Interface Contract (Spec 00)

## Summary

Two header files implement the Spec 00 interface contract:

1. **`src/surrogate_interface.h`** — Low-level math functions
   - Denormalization (log10 → physical units)
   - Quaternion canonicalization (qw ≥ 0)
   - Input validation helpers
   - Tensor preparation

2. **`src/surrogate_validator.h`** — Wrapper with LAMMPS error handling
   - Integrates interface functions with `error->one(FLERR, ...)`
   - Provides composite validation pipelines
   - Designed for use in Spec 04 (contact models)

## Key Formulas (Spec 00)

### Denormalization (R00.3)

```cpp
// Step 1: Log10 → unit scale
double overlap_unit = std::pow(10.0, (log10_output * y_scale + y_mean)) - eps;

// Step 2: Unit scale → physical
double overlap_phys = overlap_unit * L_actual;
```

For current models (run_029, run_030): y_mean=0, y_scale=1, so:
```cpp
double overlap_unit = std::pow(10.0, log10_output) - 1e-15;
```

### Quaternion Canonicalization (R00.6)

```cpp
if (quat[0] < 0.0) {
  quat[0] = -quat[0];
  quat[1] = -quat[1];
  quat[2] = -quat[2];
  quat[3] = -quat[3];
}
```

### Input Tensor Preparation (R00.1)

```cpp
// Input: [z_unit, qw, qx, qy, qz]
double z_unit = z_phys / L_actual;
input_tensor[0] = z_unit;
input_tensor[1] = quat_canonical[0];  // qw
input_tensor[2] = quat_canonical[1];  // qx
input_tensor[3] = quat_canonical[2];  // qy
input_tensor[4] = quat_canonical[3];  // qz
```

## Usage Patterns for Spec 04

### Pattern 1: Simple Denormalization

```cpp
#include "surrogate_interface.h"

using namespace LAMMPS_NS::SurrogateInterface;

// ... in contact loop ...
double log10_output = model->forward(input_tensor);
double overlap_phys = denormalize_overlap_full(
  log10_output, 
  y_mean,        // from scalers (typically 0)
  y_scale,       // from scalers (typically 1)
  L_actual,      // particle's characteristic length
  EPS_DENORM);   // 1e-15
```

### Pattern 2: Full Validation Pipeline

```cpp
#include "surrogate_validator.h"

using namespace LAMMPS_NS;

// In pair potential or fix contact calculation:
SurrogateValidator validator(error);

// Validate inputs before surrogate query
validator.validate_inputs_for_surrogate(
  z_phys,       // particle height above wall
  L_actual,     // particle's characteristic length
  quat_input,   // [qw, qx, qy, qz]
  particle_id); // for error messages

// Query surrogate (caller's responsibility)
double log10_output = model->forward(input_tensor);

// Validate outputs after surrogate query
double overlap_phys = validator.validate_outputs_from_surrogate(
  log10_output,
  z_unit,
  y_mean,
  y_scale,
  L_actual,
  particle_id);
```

### Pattern 3: Step-by-Step with Diagnostics

```cpp
#include "surrogate_interface.h"
#include "surrogate_validator.h"

using namespace LAMMPS_NS;

SurrogateValidator validator(error);

// Validate characteristic length
validator.validate_characteristic_length(L_actual, particle_id);

// Prepare input
double z_unit = z_phys / L_actual;
validator.validate_z_unit(z_unit, particle_id);

double quat[4] = {qw, qx, qy, qz};
validator.canonicalize_quaternion(quat, particle_id);

double input_tensor[5];
SurrogateInterface::prepare_input_tensor(z_phys, L_actual, quat, input_tensor);

// Query surrogate
double log10_output = model->forward(input_tensor);
validator.validate_model_output(log10_output, z_unit, particle_id);

// Denormalize
double overlap_phys = SurrogateInterface::denormalize_overlap_full(
  log10_output, y_mean, y_scale, L_actual);
validator.validate_physical_overlap(overlap_phys, particle_id);

// Use in physics model
double force = force_model->compute_force(overlap_phys, ...);
```

## Constants and Ranges

All defined in `surrogate_interface.h`:

```cpp
EPS_DENORM                    = 1e-15   // Underflow prevention
Z_UNIT_MIN_TRAIN              = -0.5    // Training range (min)
Z_UNIT_MAX_TRAIN              = 2.0     // Training range (max)
Z_UNIT_EXTRAP_THRESHOLD       = 10.0    // Extrapolation threshold (±)
LOG10_OVERLAP_MIN_TRAIN       = -5.0    // Output range (min)
LOG10_OVERLAP_MAX_TRAIN       = 0.5     // Output range (max)
```

## Error Messages (Spec 04, R04.3)

All errors prefixed with particle ID and context. Examples:

```
ERROR: Invalid characteristic_length at particle 42: L_actual=-0.5 (must be > 0 and finite)
ERROR: Invalid z_unit (NaN/Inf) at particle 17
ERROR: Invalid quaternion (contains NaN/Inf) at particle 8: [nan, 0.1, 0.2, 0.3]
ERROR: Surrogate inference produced NaN/Inf at particle 99 with z_unit=1.5
ERROR: Overflow/underflow in denormalization at particle 5: overlap_unit=nan
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

## Files Created

1. `src/surrogate_interface.h` (280 lines)
   - Low-level math functions
   - Inline implementations for performance
   - No LAMMPS dependencies

2. `src/surrogate_validator.h` (350+ lines)
   - LAMMPS integration layer
   - Error handling with `Error::one(FLERR, ...)`
   - Composite validation pipelines

## Verification

✅ Code compiles without errors  
✅ All functions are header-only (inline) — no .cpp needed  
✅ No external dependencies beyond standard C++ library  
✅ Ready for Spec 04 implementation

## Next Steps (Spec 04)

Spec 04 will use these functions to:

1. Add contact calculation in pair potential (e.g., `pair_gran_surrogate.cpp`)
2. Integrate surrogate overlap query into force model
3. Validate all inputs/outputs per Spec 04, R04.3
4. Apply dual-guard pattern throughout

Expected Spec 04 implementation locations:
- `src/pair_gran_surrogate.h/cpp` — New pair potential
- `src/contact_models.cpp` — Contact loop modification (if needed)
- Input script: `examples/LIGGGHTS/Tutorials_public/surrogate/in.surrogate_cube`

---

**Spec 00 Implementation Status: COMPLETE**

All interface functions and validators ready for Spec 04 contact model integration.
