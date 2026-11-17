# Phase 3C Implementation Handoff

**Date:** 2026-11-05  
**Status:** ✅ **READY FOR IMPLEMENTATION**

---

## Overview

Phase 3C (Surrogate Shape Integration) **specifications are complete and resolved**. All 21 QA issues have been addressed. This document shows how to proceed with implementation.

### Quick Links

- **Full Specifications:** `specs/00-surrogate-interface-contract/` through `specs/05-ctest-testing/`
- **QA Review Results:** `QA_REVIEW_PHASE3C_SPECS_20261105.md` (full analysis)
- **Executive Summary:** `QA_REVIEW_EXECUTIVE_SUMMARY.md` (quick reference)
- **Cross-Project Status:** See SS_models_training repo — `PHASE_3C_INTEGRATION_ISSUES_20261105.md`

---

## How to Initiate Implementation

### Step 1: Review the Specifications

Read the updated specs in this order (each builds on the prior):

1. **`specs/00-surrogate-interface-contract/`** (R00.1–R00.10)
   - Interface contract (inputs, outputs, denormalization)
   - Scaler format (frozen: `ss_model_record_v1`)
   - Model identity validation

2. **`specs/01-cmake-superquadric/`** (R01.1–R01.6)
   - Verify CMake setup ✅ (already complete)
   - Confirm SUPERQUADRIC_ACTIVE_FLAG present in CMakeLists.txt

3. **`specs/02-libtorch-build-integration/`** (R02.1–R02.6)
   - LibTorch CMake integration patterns
   - Model path resolution algorithm
   - Makefile.user template for legacy builds

4. **`specs/03-surro-shape-type/`** (R03.1–R03.8)
   - New `atom_style surrogate` registration
   - Input syntax: `atom_style surrogate ss-cube`
   - Shape database initialization

5. **`specs/04-ss-contact-models/`** (R04.1–R04.10)
   - Physics/geometry boundary (HARD RULE)
   - Input validation patterns
   - Contact history management

6. **`specs/05-ctest-testing/`** (RF-15 to RF-20)
   - Test tolerance adjusted to 1e-4 (matches model MAE)
   - New surrogate-specific unit/integration tests

### Step 2: Understand the Architecture

**Key Concepts:**

| Concept | Location | Purpose |
|---------|----------|---------|
| **Surrogate Models** | `SS_models/ss-cube-walls/` (run_029, run_030) | Pre-trained ML surrogates (frozen) |
| **Scaler Format** | `ss_model_record_v1` | Input/output normalization metadata |
| **Nondimensionalization** | Spec 00, Design section | Height and overlap rescaling formulas |
| **Physics/Geometry Boundary** | Spec 04, R04.1 | Surrogate handles geometry; physics models unchanged |
| **Dual-Guard Pattern** | Spec 03, R03.4 | Compile-time + runtime guards prevent crashes |

**Flow Diagram (Runtime):**

```
liggghts -in input.lmp
  ↓
parse: atom_style surrogate ss-cube
  ↓
init(): resolve SS_models/ss-cube-walls/
  ↓
load model.pt (TorchScript)
  ↓
load scalers.dat (ss_model_record_v1 format)
  ↓
During simulation (each contact):
  ↓
  prepare input: [z_unit, quat] (canonicalized)
  ↓
  query model: log10_output = model([z_unit, quat])
  ↓
  denormalize: overlap_unit = 10^(log10_output) - eps_small
  ↓
  rescale: overlap_phys = overlap_unit * L_actual
  ↓
  validate: check for NaN/Inf
  ↓
  physics model: force = compute_force(overlap_phys, ...)
```

### Step 3: Select Implementation Order

**Recommended order (dependencies):**

1. **Spec 02 first (Build):** CMake/Makefile LibTorch integration
   - Foundation for everything else
   - No runtime dependencies

2. **Spec 03 next (Data):** Shape type registration + database
   - Provides input/output infrastructure
   - Needed before contact code runs

3. **Spec 00 (Interface):** Denormalization + model loading
   - Implement once Spec 02 & 03 infrastructure is ready
   - Add scalers parser + denormalization logic

4. **Spec 04 (Physics):** Contact model integration
   - Biggest code change; touches fix_wall_gran.cpp
   - Use Spec 00, 02, 03 infrastructure

5. **Spec 01 (Verification):** CMake superquadric check
   - Already complete ✅
   - Verify dual-guard pattern works

6. **Spec 05 (Testing):** Unit + integration tests
   - Test each spec as implemented
   - Validate end-to-end pipeline

### Step 4: Mark Implementation Phase

To indicate you are starting Phase 3C implementation, **update MEMORY.md:**

```markdown
## Phase 3C Status

**Specifications:** ✅ RESOLVED (2026-11-05)  
**Implementation:** 🚀 IN PROGRESS (started 2026-11-XX)

**Implementation Order:**
1. [ ] Spec 02: LibTorch build integration
2. [ ] Spec 03: Shape type registration
3. [ ] Spec 00: Interface contract + model loading
4. [ ] Spec 04: Contact models (ss-wall)
5. [ ] Spec 01: CMake verification
6. [ ] Spec 05: Testing framework

**Commits to track progress:**
- (Will be filled as each spec is implemented)
```

### Step 5: Track Tasks Per Spec

Each spec has a `tasks.md` file with T-numbered action items:

**Example: Spec 02 tasks.md contains:**
- T2.1: CMake LibTorch integration
- T2.2: Model path resolution
- T2.3: Model and scaler loading at init()
- T2.4: Makefile integration
- T2.5: Portable error messages
- T2.6–T2.10: Testing tasks

**As you work:**
1. Mark tasks `in_progress` in the task file
2. Commit with task reference (e.g., "T2.1: Implement CMake find_package(Torch)")
3. Mark tasks `completed` when done
4. Link completed tasks in MEMORY.md

---

## Key Decisions and Constraints

### HARD RULES (Do Not Violate)

1. **Physics/Geometry Separation (Spec 04, R04.1):** Surrogate predicts all geometric outputs (overlap, contact point, normal) from a SINGLE query. Never mix surrogate overlap with native superquadric contact point.

   ```cpp
   ✓ CORRECT:   // Single surrogate query, all geometric outputs
   auto out = model.forward(input);
   overlap = denormalize(out.log10_overlap);
   contact_point = out.contact_point;    // (future)
   normal = out.normal;                   // (future)
   
   ✗ WRONG:    // Mixed sources → undefined behavior
   overlap = surrogate.forward(input);
   contact_point = superquadric_closest_point(...);
   ```

2. **Dual-Guard Pattern (Spec 03, R03.4):** Both `#ifdef SURROGATE_ACTIVE_FLAG` AND `if(atom->surrogate_flag)` must be used together.

   ```cpp
   ✓ CORRECT:
   #ifdef SURROGATE_ACTIVE_FLAG
   if(atom->surrogate_flag) {
       // Surrogate code
   }
   #endif
   
   ✗ WRONG:   // Segfaults on non-surrogate styles
   if(atom->surrogate_flag) {
       // No #ifdef → undefined behavior
   }
   ```

3. **One Shape Type Per Simulation (Spec 03, R03.6):** Runtime exclusive; cannot mix ss-cube and ss-pyramid in same run.

4. **Model Identity Validation (Spec 04, R04.5):** Always validate that particle shape_key matches loaded model before querying.

### Important Assumptions

- **Scaler Format Frozen:** `ss_model_record_v1` is the canonical format; both run_029 and run_030 use it.
- **Input Ranges:** z_unit ∈ [-10, +10] (training range); extrapolation allowed but logged.
- **Test Tolerance:** 1e-4 (matches model MAE ≈ 6.1e-5); do not use 1e-6.
- **TorchScript Export Ready:** model.pt files are ready in SS_models_training repo; do not retrain.

---

## How to Know When Phase 3C is Complete

**Definition of Done:**

- [ ] All 6 specs (00–05) have code implementations matching requirements
- [ ] All R-numbers satisfied (traced to code)
- [ ] All T-numbers marked `completed`
- [ ] All critical tests pass (Spec 05, RF-15 to RF-20)
- [ ] Contact calculation validates input, prevents NaN/Inf
- [ ] Model identity check prevents shape mismatches
- [ ] Physics/geometry boundary enforced in code
- [ ] End-to-end integration test passes (ss-cube particle vs wall with both run_029 and run_030)
- [ ] Documentation updated (BUILDING.md, input file examples)
- [ ] Commits reference specs (e.g., "Implement T2.1 (Spec 02)")

**Phase 3C Sign-off:**

When all criteria met, update MEMORY.md:

```markdown
## Phase 3C Status

**Specifications:** ✅ RESOLVED (2026-11-05)  
**Implementation:** ✅ COMPLETE (2026-11-XX)  
**Sign-Off:** All 6 specs implemented; all tests pass; physics/geometry boundary enforced.

**Ready for Phase 3D:** ss-ss contact (exploratory), contact point + normal outputs, additional shape types.
```

---

## Critical Files to Know

| File | Purpose | Read First? |
|------|---------|------------|
| `specs/00-surrogate-interface-contract/design.md` | Denormalization formula, scaler format | YES |
| `specs/02-libtorch-build-integration/design.md` | CMake pattern, path resolution algorithm | YES |
| `specs/04-ss-contact-models/requirements.md` | Physics/geometry boundary, validation patterns | YES |
| `QA_REVIEW_PHASE3C_SPECS_20261105.md` | All 21 issues traced to fixes | Reference |
| `src/style_gran_models.h` | Contact model registration (existing) | Reference |
| `src/fix_wall_gran.cpp` | Where surrogate contact code goes | Reference |

---

## Questions?

- **"How do I know what the input syntax is?"** → See Spec 03, R03.2: `atom_style surrogate ss-cube`
- **"Where do the trained models go?"** → `SS_models/ss-cube-walls/model.pt` + `scalers.dat` (Spec 02)
- **"What if the model output is NaN?"** → Error with diagnostic (Spec 04, R04.3)
- **"Can I mix superquadrics and surrogates?"** → No; compile-time selections (Spec 03, R03.5)
- **"What tolerance for tests?"** → 1e-4 for surrogates (Spec 05, NR-1)

---

## Next Checkpoint

Once you've read the specs and are ready to code:

1. **Create a branch:** `git checkout -b phase-3c-implementation`
2. **Mark MEMORY.md:** Add "Implementation: 🚀 IN PROGRESS" section
3. **Start with Spec 02 (CMake):** Edit `src/CMakeLists.txt` for LibTorch
4. **Commit:** `git commit -m "T2.1: Add LIBTORCH_DIR CMake variable and find_package(Torch)"`
5. **Track in MEMORY.md:** Update progress as you complete each T-number

---

**Summary:** Phase 3C specifications are complete, frozen, and ready for implementation. Follow the specs in order (02 → 03 → 00 → 04 → 01 → 05), enforce hard rules, and track progress via task.md files and commits. Good luck! 🚀
