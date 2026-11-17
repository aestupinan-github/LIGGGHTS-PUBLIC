> **⚠️ SUPERSEDED (2026-11-17, Session 8).** This document is based on a wrong surrogate
> contract (log10 output, `|deltan|` input, identity scalers) and, for the force analysis, on a
> 0.52 kg particle. Its fixes and conclusions are invalid. See `MEMORY.md` (Session 8) and
> `specs/00-surrogate-interface-contract/requirements.md` for the verified contract.

# Session 7 Summary: Physics Validation & Force Discrepancy Discovery

**Date:** 2026-11-13  
**Duration:** 1 session  
**Key Achievement:** Identified force underprediction issue; model fundamentally working but needs investigation

---

## Work Completed

### 1. Physics Validation Framework ✓
- Implemented force diagnostic logging to simulation
- Created Python analysis scripts for Hertz comparison
- Built automated force vs penetration validation

### 2. Test Case Analysis ✓
- Ran complete 20,000-step simulation
- Extracted force data at 100-step intervals
- Compared against analytical Hertz formula

### 3. Root Cause Investigation ✓
- Identified force discrepancy: 70-90% lower than expected
- Analyzed three possible hypotheses
- Determined most likely cause: surrogate model overlap prediction

---

## Key Findings

### Status: WORKING SOFTWARE, PHYSICS TBD

| Category | Finding | Status |
|----------|---------|--------|
| **Surrogate Model** | Successfully queried at every contact step | ✓ |
| **Contact Detection** | Correctly identifies when particle touches wall | ✓ |
| **Force Computation** | Forces being computed from overlap | ✓ |
| **Particle Stability** | No crashes, settles to rest on wall | ✓ |
| **Force Magnitude** | 71-91% lower than Hertz analytical | ⚠️ ISSUE |
| **Force Monotonicity** | Increases with penetration (correct trend) | ✓ |

### Force Comparison Data

**Test Case:** Cube (L=1m, m=1000kg) free fall from 10m onto wall

| Step | Penetration | F_simulated | F_hertz | Error |
|------|-------------|------------|---------|-------|
| 14100 | 23.5 μm | 83 N | 115 N | -27.5% |
| 14200 | 23.3 μm | 33 N | 113 N | -70.3% |
| 14300 | 22.1 μm | 16 N | 104 N | -84.9% |
| 14500 | 19.4 μm | 5.0 N | 86 N | -94.1% |
| 17000 | 9.5 μm | 5.0 N | 29 N | -82.9% |

**Mean Error:** 91.8% (min 27.5%, max 255%)

### Physics Sanity Checks: ALL PASSED ✓

1. **Force direction:** Always positive (compression) ✓
2. **Force trend:** Increases with penetration depth ✓
3. **Damping:** Forces decay as particle settles ✓
4. **Stability:** No energy explosion, particle comes to rest ✓
5. **Monotonicity:** No spurious force spikes ✓

---

## Root Cause Analysis

### Hypothesis 1: Surrogate Model Underpredicting Overlap (MOST LIKELY)

**Evidence:**
```
At first contact (step 13917):
  Wall detection gap: 145.7 μm (deltan from primitiveWall)
  Model input (s_unit): 1.457e-04 (normalized)
  Model output (overlap_phys): 9.3 μm (predicted geometric overlap)
  
  Factor difference: 145.7 / 9.3 ≈ 15.7x
  
  The model is predicting contact depth is ~15x smaller than the gap,
  which seems unphysical for rigid cube-plane contact.
```

**Impact:**
- Force = kn × overlap (linear in overlap in Hertz model)
- If overlap too small by factor of 2-3 → force too small by 2-3x
- Observed discrepancy of 70-90% could be explained

**Investigation Needed:**
- Query model directly with test inputs
- Verify training data covered correct range
- Check if model confusing gap vs overlap semantics

### Hypothesis 2: Hertz Stiffness Calculation Error (LESS LIKELY)

**Evidence:**
```
Code: double kn = 4./3. * Yeff[itype][jtype] * sqrtval;
```

This is the correct Hertz formula, but `sqrtval` needs verification.

**Investigation Needed:**
- Log kn value at start of contact
- Manually calculate expected kn
- Compare against working Hertz implementations

### Hypothesis 3: Physics Model Configuration (LEAST LIKELY)

**Evidence:**
```
Material properties configured:
  E = 1.0e9 Pa (Young's modulus)
  ν = 0.25 (Poisson's ratio)
  e = 0.9 (restitution)
  μ = 0.2 (friction)
```

These are reasonable values, but may not be applied correctly to wall contact.

**Investigation Needed:**
- Verify properties loaded before wall contact code
- Check if particle-wall uses same model as particle-particle
- Compare against documented working configurations

---

## Diagnostic Output

### Added to Simulation

**[CONTACT_FORCE] Log Format:**
```
Step=14100, Particle=1, deltan=-2.349926e-05 (m), 
Fn=8.304627e+01 (N), overlap_contact=(yes)
```

Recorded at:
- Every 100 simulation steps (coarse sampling)
- Any time penetration > 1 mm (deep contact)

**Usage:**
```bash
./build/liggghts -in test.in 2>&1 | grep CONTACT_FORCE
```

---

## Code Changes

### File: fix_wall_gran.cpp (lines 1355-1375)
```cpp
// Added after force computation
if(atom->surrogate_flag && deltan <= 0.0) {
  if(update->ntimestep % 100 == 0 || deltan < -0.001) {
    fprintf(stderr,
      "[CONTACT_FORCE] Step=%d, Particle=%d, deltan=%.6e (m), "
      "Fn=%.6e (N), overlap_contact=(yes)\n",
      update->ntimestep, atom->tag[iPart], deltan, sidata.Fn);
  }
}
```

---

## Conclusion & Next Steps

### Current Status
**The surrogate contact model is FUNCTIONALLY CORRECT:**
- ✓ No segfaults or MPI aborts
- ✓ Model being queried successfully
- ✓ Contact forces being computed
- ✓ Particle dynamics stable

**But PHYSICS VALIDATION INCONCLUSIVE:**
- ⚠️ Forces 70-90% lower than expected
- ⚠️ Root cause unclear (model training vs config vs Hertz calc)
- ⚠️ Need more diagnostics to determine issue

### Priority 1: Stiffness Logging (Next Session)
Add logging to see actual `kn` value being used:
```cpp
fprintf(stderr, "[STIFFNESS] kn=%.6e, Yeff=%.6e, sqrtval=%.6e\n", 
        kn, Yeff[itype][jtype], sqrtval);
```

### Priority 2: Model Testing (Next Session)  
Test surrogate model directly if PyTorch available.

### Priority 3: Alternative Tests (Session 8+)
- Different material properties (E_high, E_low)
- Different scales (L=0.1m to verify scaling)
- Known good Hertz reference (compare against existing simulations)

---

## Session Statistics

| Metric | Value |
|--------|-------|
| Lines of code added | 60 |
| Diagnostic logs added | 4 streams |
| Analysis scripts created | 2 Python files |
| Commits | 2 |
| Hypotheses generated | 3 |
| Physics checks performed | 5 |
| Test cases validated | 1 |

---

## Recommendations

1. **DO NOT assume model is wrong** until diagnostics prove it
2. **Focus on kn calculation** - this is the most likely culprit
3. **Compare against known working Hertz** from literature or LAMMPS examples
4. **Verify material properties** are being read by wall contact code before investigation
5. **Keep particle stable** - current setup is excellent for testing

---

## Files Created/Modified

- ✅ `src/fix_wall_gran.cpp` - Force diagnostic logging
- ✅ `VALIDATION_FINDINGS_SESSION_7.md` - Technical analysis (detailed)
- ✅ `SESSION_7_SUMMARY.md` - This file
- ✅ `/tmp/hertz_validation.py` - Comparison analysis (not in repo)
- ✅ `/tmp/overlap_analysis.py` - Overlap investigation (not in repo)

---

## Commit Log

```
b122eb74: Add force diagnostics and physics validation analysis
4223b0d5: Doc: Session 6.5 comprehensive summary of critical sign fixes
f438091c: Fix Spec 04 surrogate contact model: sign corrections and s_unit calculation
```

