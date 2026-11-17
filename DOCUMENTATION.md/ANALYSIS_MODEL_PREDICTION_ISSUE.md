> **⚠️ SUPERSEDED (2026-11-17, Session 8).** This document is based on a wrong surrogate
> contract (log10 output, `|deltan|` input, identity scalers) and, for the force analysis, on a
> 0.52 kg particle. Its fixes and conclusions are invalid. See `MEMORY.md` (Session 8) and
> `specs/00-surrogate-interface-contract/requirements.md` for the verified contract.

# Analysis: Surrogate Model Prediction Issue - Session 6.5

**Date:** 2026-11-13  
**Critical Finding:** Model predicts positive overlap (gap) even when particle should be in contact

---

## Issue Summary

The surrogate model is predicting non-physical results:
- When `s_unit ≈ 0.5` (particle at wall surface), model predicts `overlap_phys ≈ +0.014 m`
- Positive overlap means "gap exists", so no contact force is calculated
- Particle falls through wall without experiencing contact resistance
- Particle deleted when it falls below z=0 (fixed boundary condition)

**Expected Behavior:**
- `s_unit = 0.5`: overlap ≈ 0 (just touching)
- `s_unit = 0.0`: overlap < 0 (penetration)
- `s_unit = -0.1`: overlap ≈ -0.05 m (deeper penetration)

**Actual Behavior:**
```
Step 14183: s_unit = 0.499, overlap = +0.0141 m (gap predicted, no contact)
Step 14184: s_unit = 0.498, overlap = +0.0137 m (still gap)
Step 14200: s_unit = 0.476, overlap = +0.0093 m (gap continues)
```

---

## Diagnostic Data

### Model Output Log (first contact steps)

```
[MODEL_OUTPUT] Step=14183, s_unit=4.992252e-01, overlap_phys=1.407151e-02, gap_predicted=1
[MODEL_OUTPUT] Step=14184, s_unit=4.978338e-01, overlap_phys=1.373360e-02, gap_predicted=1
[MODEL_OUTPUT] Step=14185, s_unit=4.964423e-01, overlap_phys=1.340386e-02, gap_predicted=1
...continues with positive overlap_phys throughout...
```

### Observed Timeline

| Step | s_unit | overlap_phys (m) | Contact? | Status |
|------|--------|-----------------|----------|--------|
| 14183 | 0.4992 | +0.01407 | NO | Approaching wall |
| 14200 | 0.4756 | +0.00931 | NO | Should touch, but gap predicted |
| 14250 | 0.3727 | +0.00180 | NO | Deep into wall, still gap! |
| 14278 | 0.3680 | ~0.0015 | NO | Last SURROGATE_APPROACH output |
| 14300 | N/A | N/A | DELETED | Atom count = 0 |

### Physics Expectation vs. Reality

**Hertz Contact Model Reference:**
- For sphere of radius R touching plane at depth δ:
  - δ = 0: F = 0 (just touching)
  - δ = 0.01 m: F ≈ k·δ^(3/2) >> 0 (significant force)
  - δ = 0.05 m: F ≈ large (strong contact)

**Surrogate Model Behavior:**
- s_unit = 0.5, overlap = +0.014 m: **gap predicted** → F = 0
- s_unit = 0.37, overlap = +0.001 m: **still gap** → F = 0
- Result: **No contact force throughout contact phase**

---

## Root Cause Analysis

### Hypothesis 1: Denormalization Formula Wrong
**Current Code (surrogate_interface.h:59-63):**
```cpp
inline double denormalize_overlap(double log10_output, double y_mean, double y_scale, double eps)
{
  double overlap_unit = std::pow(10.0, (log10_output * y_scale + y_mean)) - eps;
  return overlap_unit;
}
```

**Issue:** For training data with y_mean=0, y_scale=1 (current models):
- Formula: `overlap_unit = 10^(log10_output) - 1e-15`
- If model outputs `log10_output = -2.0`: `overlap_unit = 10^(-2) ≈ 0.01` m
- This is positive (gap), but model should output **negative** overlap for contact!

**Interpretation Problem:**
- Training data likely: overlap ∈ [-1.0, +0.5] (signed, negative = penetration)
- Denormalized as: 10^(log10_overlap) - eps
- But if training overlap ranges include both positive and negative values, denormalization doesn't work!

### Hypothesis 2: Model Training Output is Wrong Sign
- Model trained to output `log10(|overlap|)` with separate sign bit
- C++ code interprets as `10^(output)` which is always positive
- Model never produces negative overlaps

### Hypothesis 3: s_unit Calculation Still Wrong
- Even with signed centre_distance, maybe input still wrong
- Model might expect: `s_unit = (z_centre - z_wall) / L` (not `(deltan + r) / L`)
- If z coordinates differ by sign convention, inputs may be inverted

---

## Verification Tests

### Test 1: Check Model Directly
```python
import torch
model = torch.jit.load("SS_models/ss-cube-walls/model.pt")

# Test with training range inputs
s_unit_vals = [-0.1, 0.0, 0.1, 0.5]
for s in s_unit_vals:
    pos_z = torch.tensor([[s]], dtype=torch.float64)
    quat = torch.tensor([[1.0, 0.0, 0.0, 0.0]], dtype=torch.float64)
    output = model(pos_z, quat)
    log10_output = output[0].item()
    overlap = 10^(log10_output)
    print(f"s_unit={s}: log10_output={log10_output:.4f}, overlap={overlap:.6f}")
```

Expected output for contact model:
- s_unit = 0.5: log10_output ≈ -2.0 (overlap ≈ 0.01, ~at surface)
- s_unit = 0.0: log10_output ≈ -0.5 (overlap ≈ -0.32, deep penetration)
- s_unit = -0.1: log10_output ≈ 0.0 (overlap ≈ 1.0, very deep penetration)

**IF model outputs are positive (gap) for all inputs:**
- Model is trained backwards (gap prediction mode, not contact mode)
- OR denormalization formula is wrong
- OR input coordinate system is wrong

### Test 2: Analytical Comparison
```
For L=1.0m cube, E=1.0e9 Pa, ν=0.25:

At contact touchdown (s_unit ≈ 0.5):
- Geometrical overlap = 0 (just touching)
- Elastic deformation ≈ 0.001-0.01 m (small elastic compression)
- Expected overlap ∈ [-0.01, 0.0]

Model prediction:
- overlap_phys = +0.014 m (gap, contradiction!)
```

---

## Impact on Simulation

### Why Particle Disappears

1. **No Contact Force:** Model predicts gap → skip contact logic → F_contact = 0
2. **Free Fall Continues:** Particle accelerates downward unimpeded
3. **Boundary Deletion:** Particle z drops below z=0 → deleted (boundary f f f)
4. **Particle Gone:** By step 14300, atom count = 0

### Why Visible in Diagnostics

- SURROGATE_APPROACH: Executed for each step (model queried)
- MODEL_OUTPUT: Shows positive overlap for all steps
- SURROGATE_CONTACT: NEVER executed (caught by `if(overlap_phys > 0.0) continue;` at line 1265)

---

## Solutions (Priority Order)

### Fix 1: Verify Model Output Sign (CRITICAL)
```cpp
// In surrogate_contact.h query_overlap(), add early diagnostic:
fprintf(stderr, "[MODEL_RAW_OUTPUT] log10_output=%.6f, before_denorm\n", log10_output);
double overlap_phys = validator.validate_outputs_from_surrogate(...);
fprintf(stderr, "[MODEL_DENORM_OUTPUT] overlap_phys=%.6e, after_denorm\n", overlap_phys);
```

**Action:** Run with diagnostic, confirm if issue is in model or denormalization

### Fix 2: Check Denormalization Formula
```cpp
// If y_mean/y_scale are defaults, try alternative:
double overlap_alt = -std::pow(10.0, log10_output);  // Try negative
// Or check if output is already scaled differently
```

### Fix 3: Invert s_unit Sign
```cpp
// Try negative s_unit:
double s_unit_alt = -centre_distance / L_actual;  // Inverted sign
```

### Fix 4: Retrain Model
- If model is fundamentally trained backwards
- Need to retrain with correct labels (negative overlap = penetration)
- Or use negative of model output

---

## Next Session Priorities

1. **Add diagnostic output after model forward pass** (raw log10 output)
2. **Test model directly in Python** to see actual outputs
3. **Check if denormalization is the culprit** (invert sign, add prefix minus)
4. **If model is correct but inverted:** easy fix in denormalization
5. **If model is trained wrong:** deferred to ML training team

---

## Files to Modify

### For Diagnostics
- `src/surrogate_contact.h` (lines 190-227): Add raw_log10_output diagnostic

### For Potential Fixes
- `src/surrogate_validator.h` (lines 294-310): Try negating overlap_unit
- `src/fix_wall_gran.cpp` (line 1215): Try s_unit_alt = -s_unit

---

## Conclusion

**The particle disappearance is NOT a timestep issue or force magnitude issue.**

**Root Cause:** Model predicts positive overlap (gap) even when physically in contact, preventing contact force calculation.

**Status:** Requires model output verification and likely a simple sign fix in denormalization or input coordinate system.

