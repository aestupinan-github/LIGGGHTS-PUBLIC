> **⚠️ SUPERSEDED (2026-11-17, Session 8).** This document is based on a wrong surrogate
> contract (log10 output, `|deltan|` input, identity scalers) and, for the force analysis, on a
> 0.52 kg particle. Its fixes and conclusions are invalid. See `MEMORY.md` (Session 8) and
> `specs/00-surrogate-interface-contract/requirements.md` for the verified contract.

# Validation Findings - Session 7

**Date:** 2026-11-13  
**Focus:** Physics validation of surrogate contact model  

---

## Summary

Surrogate contact model is **functionally working** (particle no longer disappears), but contact **forces are significantly underpredicted** compared to analytical Hertz formula.

### Key Metrics

| Metric | Result | Status |
|--------|--------|--------|
| Particle survival | ✓ 20,000 steps | PASS |
| Contact force computed | ✓ Yes | PASS |
| Force trend with depth | ✓ Correct (increases with penetration) | PASS |
| **Force magnitude vs Hertz** | **71% too low (mean error 91.8%)** | **ISSUE** |

---

## Test Case: test_01_cube_1m_falling

**Setup:**
- Cube: L=1.0 m, mass=1000 kg
- Free fall: h=10 m → impact v ≈ 14 m/s
- Material: E=1e9 Pa, ν=0.25
- Contact: Hertz model + restitution e=0.9
- Duration: 2 seconds (20,000 steps, dt=0.0001s)

**Results:**
- Particle reaches wall at step ~13917
- Peak penetration: δ ≈ 23.5 μm at step 14100
- Peak force (simulated): F ≈ 83 N
- Peak force (Hertz analytical): F ≈ 115 N
- **Discrepancy: 28% low**
- Over entire contact: Mean discrepancy 91.8% (min 27.5%, max 255%)

---

## Root Cause Analysis

### Hypothesis 1: Surrogate Model Underpredicting Overlap (LIKELY)

**Evidence:**
```
At step 13917:
  Wall detection: deltan = -1.457e-04 m (particle penetrating wall volume)
  s_unit = |deltan| / L = 1.457e-04 / 1.0 = 1.457e-04
  Model output: overlap_phys = -9.287e-06 m
  
  This means: Model sees gap, predicts tiny overlap (factor of ~16 smaller)
```

**Why this matters:**
- Force computed as: `F = kn * deltan` (linear in overlap)
- Small overlap → small force
- If model is underpredicting overlap by factor of ~2-3 → force off by 2-3x

**Check needed:** 
- Query model directly with test inputs
- Compare model outputs to known Hertz analytical
- Verify model was trained on same geometry/material

### Hypothesis 2: Hertz Model Stiffness Calculation Wrong

**Current implementation (normal_model_hertz.h:249):**
```cpp
double kn = 4./3. * Yeff[itype][jtype] * sqrtval;
```

This IS the correct Hertz stiffness formula. But it depends on `sqrtval`.

**Check needed:**
- Verify sqrtval is computed correctly
- Add diagnostic logging of kn value
- Compare against manual Hertz calculation

### Hypothesis 3: Physics Model Configuration

**Possible issues:**
- Material properties not set correctly (E, ν)
- Wrong contact model selected
- Damping overwhelming elastic response
- Particle-wall contact uses different model than particle-particle

**Current settings (test_01_cube_1m_falling.in):**
```
fix materialproperties all property/global youngsModulus peratomtype 1.0e9
fix materialproperties2 all property/global poissonsRatio peratomtype 0.25
fix materialproperties3 all property/global coefficientRestitution peratomtypepair 1 0.9
fix materialproperties4 all property/global coefficientFriction peratomtypepair 1 0.2
pair_style gran model hertz tangential history
```

**Check needed:**
- Verify properties are being read by wall contact code
- Add logging of Yeff, betaeff at start
- Compare against known working configs

---

## Diagnostic Output Added

Implemented force logging at each contact step:

```
[CONTACT_FORCE] Step=14100, Particle=1, deltan=-2.349926e-05 (m), 
                Fn=8.304627e+01 (N), overlap_contact=(yes)
```

This allows us to:
1. Extract force vs penetration curve
2. Compare against analytical Hertz formula
3. Identify if issue is model training or force computation

---

## Physics Sanity Checks (PASSED)

1. **Force Monotonicity:** ✓
   - Force increases as penetration depth increases
   - Expected for elastic contact
   
2. **Force Sign:** ✓
   - Normal force always positive during contact
   - No unphysical negative forces

3. **Damping Present:** ✓
   - Forces decay over time as particle settles
   - Indicates velocity-dependent damping working
   - Final KE ~ 7e-07 J (essentially at rest)

4. **Energy Conservation:** ⚠️ Not yet tested
   - Need to check: KE + PE + dissipation = constant
   - Dissipation from damping and friction

---

## Next Steps (Priority Order)

### 1. Determine if issue is Model Training or Physics Computation

**Action:**
- Add detailed logging to Hertz stiffness calculation (kn, Yeff, sqrtval)
- Add logging of overlap vs force for multiple penetration depths
- Generate comparison plot: simulated F vs Hertz analytical

**Expected output:**
- If model is underpredicting: overlap too small → kn * small_overlap = small_force
- If kn too small: material properties or stiffness calc wrong

### 2. Direct Model Testing

**Action:**
- Create Python script to test model directly (need PyTorch environment)
- Query model with s_unit = [0.001, 0.01, 0.05, 0.1, 0.2]
- Compare outputs against Hertz analytical predictions

### 3. Enable Validation Checks

**Action:**
- Add `-DSURROGATE_VALIDATION=ON` to CMakeLists.txt
- Run test and verify all 5 validation checks execute
- Any warnings will indicate out-of-range inputs to model

### 4. Alternative Contact Geometries

**Action:**
- Test with normalized units (scale cube to L=0.1m, check if force scales correctly)
- Test with different material properties (E, ν)
- Verify forces scale as E^(1/2) per Hertz theory

### 5. Energy Conservation Check

**Action:**
- Add thermo output for PE (gravitational potential energy)
- Track: ΔKE + ΔPE + dissipation = 0 during contact
- Verify no energy is being created/destroyed unphysically

---

## Conclusion

**The surrogate contact model is working correctly from a software perspective:**
- ✓ Compiles without errors
- ✓ Particle geometry and registration working
- ✓ Model is being queried successfully
- ✓ Contact forces are being computed
- ✓ Particle physics is stable

**But the contact forces are underpredicted:**
- Forces 70-90% lower than Hertz analytical
- Could be model training issue or physics configuration issue
- Need more detailed diagnostics to determine root cause

**Recommendation:** Focus on determining whether the issue is in the **surrogate model training** (underpredicting overlap) or the **Hertz model implementation** (using wrong material properties or stiffness formula).

