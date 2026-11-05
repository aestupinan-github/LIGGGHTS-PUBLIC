# QA Review — Phase 3C Specifications (Surrogate Shape Integration)

**Date:** 2026-11-05  
**Reviewer:** OpenCode QA Agent (Spec-Driven Development Mode)  
**Scope:** Specs 00–05 readiness assessment  
**Status:** ❌ **NOT READY FOR IMPLEMENTATION** — 21 significant problems identified  
**Review Mode:** Professional QA + Scientific Software Assessment

**Related:** This review references deliverables from `/home/su_estupinan@private.list.lu/Programs/SS_models_training/` (Phase 3B complete).

---

## Executive Summary

The five Phase 3C specifications (00: Interface Contract, 01: CMake Build, 02: LibTorch Integration, 03: Shape Type, 04: Contact Models, 05: Testing) contain structural gaps, ambiguities, and contradictions that will lead to inconsistent implementations and silent model mismatches at runtime.

**Critical Issues (Block Implementation):**
1. **Scaler format mismatch** — Spec 00 prescribes format; deliverable uses different format
2. **Output denormalization ambiguous** — Formula lacks operator precedence; contact forces will differ
3. **Model specificity not validated** — No runtime check that input shape matches training shape
4. **Contact physics not specified** — Risk of mixing geometric/physics sources (known bug vector)

**Structural Issues (Require Redesign):**
5. Scaler format "frozen" state has no objective criteria
6. Input feature validation (ranges, edge cases) missing
7. Model path resolution (abs/rel, env vars) undefined
8. Shape database initialization (input syntax) not specified
9. Test tolerance (1e-6) is 16× tighter than model accuracy (6.1e-5)

**Missing Specifications (Incomplete Domain Coverage):**
10. Contact point/normal output format (needed for future ss-ss)
11. Model versioning and compatibility
12. Error recovery and fallback logic
13. Units/coordinate system integration
14. Multi-material/multi-interaction support
15. Surrogate-specific test coverage

---

## Detailed Findings

### **CRITICAL ISSUES — Block Implementation**

#### **Problem 1: [Scaler Format Mismatch]**

**Severity:** CRITICAL  
**Affected Specs:** 00 (requirements), 02 (design), downstream implementations

**Details:**

Spec 00 requirements (lines 23–36) prescribe a simple key-value format:
```
y_mean
<value>
y_scale
<value>
eps
<value>
```

The actual deliverable from SS_models_training (run_029, run_030) uses an **extended key-value format** with version metadata:
```
format ss_model_record_v1
run_id run_029
model_type option4
input_dtype float64
input_units canonical_L1
y_mean 0.0
y_scale 1.0
r_circ 0.866025403784
z_prefilter 1.0
sd_too_deep -0.1
h_exact_method analytical_rotation_matrix
network_input_dim 5
ref_eval one_row_per_call_float64_cpu
ref_sd_tol 1e-14
n_ref 0
```

Spec 00 (line 36) acknowledges this: *"Current scalers.dat files have 4 values per line (likely from a multi-output model with normals). The model is not frozen yet — this format must be revisited when the model is frozen."*

**Impact:**
- Parser implementations in Spec 02 (CMake/Makefile) and Spec 04 (contact model code) will handle the **actual** format, not the prescribed one.
- Code written to Spec 00 will fail on real models.
- No versioning; if format changes again, backward compatibility is undefined.

**Cross-Link to Training Repo:**
- See: `/home/su_estupinan@private.list.lu/Programs/SS_models_training/interactions/ss_cube-wall/output/run_029/scalers.dat`
- See: `/home/su_estupinan@private.list.lu/Programs/SS_models_training/interactions/ss_cube-wall/output/run_030/scalers.dat`

---

#### **Problem 2: [Output Denormalization Formula Ambiguous]**

**Severity:** CRITICAL  
**Affected Specs:** 00 (design), 04 (implementation)

**Details:**

Spec 00 design (line 16) specifies:
```
Denormalization: overlap_unit = 10^(output * y_scale + y_mean) - eps
```

This formula has **ambiguous operator precedence**. Possible interpretations:
- **A:** `overlap_unit = (10^(output * y_scale + y_mean)) - eps`  (most likely)
- **B:** `overlap_unit = 10^((output * y_scale + y_mean) - eps)`  (unusual)
- **C:** `overlap_unit = ((10^(output * y_scale + y_mean)) - eps)` (same as A, verbose)

Furthermore:
- Role of `eps` unclear: Is it a post-processing safety constant? Or part of the log transform?
- In practice, the training code (run_029, run_030) uses `y_mean=0.0, y_scale=1.0, eps` undefined in usage.
- No specification for what `eps` value to use in LIGGGHTS (it appears in scalers.dat but is never populated: `n_ref=0`).

**Impact:**
- Different implementations will denormalize differently.
- Contact forces computed from different overlap values.
- Violates CONSTITUTION.md principle 1: "Scientific accuracy — never guess at physical transforms."

**Cross-Link to Training Repo:**
- See: `/home/su_estupinan@private.list.lu/Programs/SS_models_training/PHASE_3B_COMPLETE.md` (documents Option 4 and Option 1 inference pipelines with denormalization code)
- Training repo specifies: `overlap_unit = 10^output - 1e-15` (for log-space predictions)

---

#### **Problem 3: [Model Specificity Not Validated at Runtime]**

**Severity:** CRITICAL  
**Affected Specs:** 00 (requirements), 03 (shape database), 04 (contact implementation)

**Details:**

Spec 00 (line 43) states:
> "Each model is trained on a specific (shape, object) pair. Example: `ss-cube-walls`, `ss-piramide-walls`, `ss-cube-ss-cube`"

However:
- The input tensor format `[z_unit, qw, qx, qy, qz]` does **not include** or validate shape identity.
- No specification for runtime check that the shape in `atom->shape_key` matches the model's training shape.
- No error handling if a user provides shape_key=3 (pyramid) but loads the cube model.

**Impact:**
- Silent model mismatches at runtime.
- Querying cube model with pyramid geometry produces invalid contact (garbage in/garbage out).
- Users have no diagnostics; simulation either crashes or produces physically nonsensical results.

**Example Failure:**
```
Input script declares: atom_style surrogate ss-pyramid
Loads model: SS_models/ss-pyramid-walls/overlap_model.pt
But initialization error or user mistake causes: ss-cube model loaded instead
Runtime: Pyramid particle queries cube model silently → invalid contact geometry
→ Unphysical forces, energy gain, simulation divergence
```

---

#### **Problem 4: [Contact Physics Boundary Not Enforced]**

**Severity:** CRITICAL  
**Affected Specs:** 04 (requirements/design), 05 (testing)

**Details:**

Spec 04 (lines 6–8) emphasizes:
> "The surrogate replaces the **geometric** contact calculation (overlap, contact point, normal). **Physics** contact models (Hertz, Hooke, etc.) remain unchanged."

and (from CONSTITUTION.md rule 1):
> "Overlap AND contact point/normal MUST come from the same consistent geometric evaluation."

However, Specs 04 does **not specify**:
- How to prevent code from mixing surrogate overlap with natively-computed contact point/normal.
- What to do if surrogate predicts `overlap > 0` (unphysical; should be ≤0 for actual contact).
- How contact history transitions are validated (energy/momentum conservation across deltan sign change).
- Numerical stability near `deltan = 0` (contact breaking).

**Impact:**
- Implementation may reuse superquadric closest-point logic for contact point while using surrogate for overlap.
- This mismatch was identified as a likely cause of energy-gain/non-physical-bounce bugs in prior discarded work (MEMORY.md line 16).
- Contact forces will be inconsistent: overlap from one source, normal from another.

---

### **STRUCTURAL ISSUES — Require Redesign**

#### **Problem 5: [Scaler Format "Frozen" State Has No Objective Criteria]**

**Severity:** HIGH  
**Affected Specs:** 00 (requirements)

**Details:**

Spec 00 (line 36) defers scaler format finalization to "when the model is frozen":
> "**Note**: Current scalers.dat files have 4 values per line (likely from a multi-output model with normals). The model is not frozen yet — this format must be revisited when the model is frozen."

However:
- No definition of "frozen" state.
- No objective completion criteria for when scaler format is locked.
- No versioning scheme for format changes.
- No specification for backwards-compatibility if format changes post-implementation.

**Impact:**
- "Frozen" is vague; the model may never be considered frozen.
- If format changes, all LIGGGHTS installations must be redeployed.
- No upgrade path specified.

---

#### **Problem 6: [Missing Input Feature Validation Specification]**

**Severity:** HIGH  
**Affected Specs:** 00 (requirements), 04 (implementation)

**Details:**

Spec 00 specifies input tensor `[z_unit, qw, qx, qy, qz]` but does **not** specify:
- Valid ranges for `z_unit` (e.g., `z_unit ∈ [-10, +10]`? or unbounded?).
- Behavior if `qw < 0` (Spec 00 line 20 says canonicalize, but silent or warn?).
- Behavior if quaternion is not unit-normalized (||q|| ≠ 1).
- Behavior if particle is deeply interpenetrating (`z_unit < sd_too_deep = -0.1`).
- What constitutes an "invalid" query (NaN? Inf? Out of training range?).

**Impact:**
- At runtime, surrogate query may produce NaN, Inf, or garbage.
- No diagnostics; forces computed from invalid geometry.
- Silent failures are likely; simulation diverges without clear cause.

---

#### **Problem 7: [Model Path Resolution Undefined]**

**Severity:** HIGH  
**Affected Specs:** 02 (design), implementation

**Details:**

Spec 02 design (lines 27–39) specifies folder structure:
```
SS_models/
  ss-cube-walls/
    overlap_model.pt
    scalers.dat
```

But does **not** specify:
- Absolute vs. relative paths (relative to what? Build dir? Install dir? Working dir?).
- Environment variables or config files for locating SS_models root.
- What happens if folder does not exist at init time (fail immediately? Try to download? Silent fallback?).
- Installation/deployment: where should SS_models/ be placed in user's system?

**Impact:**
- Users cannot reliably deploy LIGGGHTS+surrogates across systems.
- Path resolution failures are cryptic (file not found, segfault, or silent use of wrong model).
- CI/CD and containerization become difficult.

---

#### **Problem 8: [Shape Database Initialization Syntax Not Specified]**

**Severity:** HIGH  
**Affected Specs:** 03 (design), 04 (implementation)

**Details:**

Spec 03 design (lines 31–39) specifies:
> "At init: read shape names from input file, populate vectors"

However, does **not** specify:
- Input file syntax to declare shape names (e.g., `atom_style surrogate ss-cube` vs. `atom_style surrogate` with separate shape declaration?).
- What constitutes a valid shape name (alphanumeric only? underscores? dashes? max length?).
- Behavior if same shape name is declared twice (error? warning? use first?).
- Maximum number of shapes supported.
- Where shape name → shape key mapping is stored/logged.

**Impact:**
- Input file format is a guess; implementations will differ.
- Parser errors are cryptic (syntax error vs. semantic error?).
- Users cannot reliably specify surrogate shapes in scripts.

---

#### **Problem 9: [Test Tolerance Mismatched to Model Accuracy]**

**Severity:** HIGH  
**Affected Specs:** 05 (requirements)

**Details:**

Spec 05 requirements (line 66) specifies:
> "THE SYSTEM SHALL use a default tolerance of 1e-6 for integration tests unless overridden per-test."

However:
- Model accuracy (run_029): 84.7% @ 1e-4, MAE = 6.1e-5
- Model accuracy (run_030): 55.3% @ 1e-4, MAE = 7.6e-4
- Default test tolerance (1e-6) is **16–17× tighter** than the best model's mean absolute error.

**Impact:**
- Integration tests will systematically fail on valid predictions from run_029.
- Either the tolerance is wrong, or model selection is wrong.
- Tests cannot verify that surrogate integration works correctly.

---

### **MISSING SPECIFICATIONS — Incomplete Domain Coverage**

#### **Problem 10: [Contact Point/Normal Output Format Undefined]**

**Severity:** MEDIUM  
**Affected Specs:** 00 (design), 04 (implementation)

**Details:**

Spec 00 (design, lines 19–23) mentions future extension to include contact point and normal:
```
[log10(overlap_unit), contact_point_x, contact_point_y, contact_point_z, normal_x, normal_y, normal_z]
```

But does **not** specify:
- Coordinate system for contact point (local to particle? local to wall? global?).
- Contact normal orientation convention (away from wall? away from particle? consistent across shape types?).
- What happens if surrogate predicts contact point outside shape domain.
- How to validate contact normal magnitude (||n|| = 1?).

**Impact:**
- Future ss-ss contact (Spec 04 line 39) cannot be consistently implemented.
- Contact point/normal will be inconsistent across geometric branches (ss-wall vs ss-ss).

---

#### **Problem 11: [Model Versioning and Compatibility Not Specified]**

**Severity:** MEDIUM  
**Affected Specs:** 00, 02, 04 (all)

**Details:**

Specs do **not** specify:
- Where model format version goes (TorchScript version? scalers.dat version? metadata?).
- How LIGGGHTS checks if a model is compatible with the current code version.
- What to do if user provides a model trained with old Python training code.
- Backwards compatibility: old models → new LIGGGHTS?  new models → old LIGGGHTS?
- Version mismatch error messages and recovery.

**Impact:**
- Users can accidentally load incompatible models.
- Silent mismatches (wrong physics) or crashes.
- No upgrade path; simulations break after code updates.

---

#### **Problem 12: [Error Recovery and Fallback Logic Not Specified]**

**Severity:** MEDIUM  
**Affected Specs:** 02, 03, 04

**Details:**

AGENTS.md (training repo, line 10) mentions Extended Option 1 (run_030) as "**FALLBACK**" model. However:
- No specification for when/how to switch to fallback at runtime.
- No specification for detecting that primary model (run_029) is producing invalid results.
- No specification for user-controlled model selection at init time (primary vs. fallback).
- No specification for whether fallback is automatic or requires user intervention.

**Impact:**
- Runtime degradation is not possible; if primary model fails, simulation crashes.
- Users cannot choose accuracy/speed tradeoffs at runtime.

---

#### **Problem 13: [Units and Coordinate System Integration Not Specified]**

**Severity:** MEDIUM  
**Affected Specs:** 00 (requirements), 04 (implementation)

**Details:**

Spec 00 (lines 14–17) specifies:
> "Train once at canonical L=1. At query time, rescale by particle's own characteristic length: `overlap_phys = overlap_unit * L_actual`"

But does **not** specify:
- LIGGGHTS unit system (real? metal? micro? custom?).
- How canonical L=1 relates to LIGGGHTS simulation units.
- What happens if simulation uses non-SI units.
- What constitutes the "characteristic length" L_actual (particle radius? diameter? half-edge?).
- What happens if L_actual = 0 or L_actual < 0.

**Impact:**
- Unit mismatches are silent and catastrophic.
- A simulation in angstroms (Å) using a model trained for meters produces contact forces off by orders of magnitude.
- Rescaling behavior is undefined; contact forces are unpredictable.

---

#### **Problem 14: [Multi-Material and Multi-Interaction Support Undefined]**

**Severity:** MEDIUM  
**Affected Specs:** 03 (design), 04 (implementation)

**Details:**

Spec 03 (line 55) states:
> "one shape type per simulation (like superquadrics/multisphere)"

However:
- No specification for loading multiple shape types simultaneously (ss-cube AND ss-pyramid in same run).
- No specification for model selection with multiple shape types.
- No specification for name collisions (two shapes with overlapping object types?).

**Impact:**
- Only one surrogate shape type supported per run.
- Multi-interaction feature (stated goal in CONSTITUTION.md and AGENTS.md) is blocked.
- Cannot scale to realistic DEM with multiple particle types.

---

#### **Problem 15: [Surrogate-Specific Test Coverage Not Specified]**

**Severity:** MEDIUM  
**Affected Specs:** 05 (design, tasks)

**Details:**

Spec 05 defines generic CTest infrastructure but does **not** specify:
- Unit tests for quaternion canonicalization (required by CONSTITUTION.md, not in tasks).
- Unit tests for nondimensionalization rescaling (required by Spec 00, not in tasks).
- Integration tests that exercise full surrogate inference pipeline (load model → canonicalize input → call forward → denormalize output).
- Regression tests comparing LIGGGHTS surrogate forces against analytical or experimental reference.
- Tests for error conditions (invalid input, missing model, incompatible format).

**Impact:**
- Tests will not catch surrogate-specific bugs.
- Integration spec cannot proceed without explicit test coverage.

---

### **Dependency and Status Issues**

#### **Problem 16: [Spec 01 (CMake Superquadric Fix) Completion Status Unclear]**

**Severity:** MEDIUM  
**Affected Specs:** 01, 02–04 (depend on working CMake build)

**Details:**

- MEMORY.md (lines 37–41) describes "SQ Build Issue (RESOLVED)" with fix applied.
- However, Spec 01 `tasks.md` is missing or empty.
- No verification that fix is in current code or that superquadric tests pass.

**Impact:**
- Spec 02+ assume working CMake build; if Spec 01 is not actually complete, they will fail.
- Dependency order is unclear to implementation team.

---

## Assessment by Specification

### **Spec 00: Surrogate Interface Contract**
- **Status:** Draft, not ready
- **Readiness:** 40% (core concept clear, but critical details ambiguous or deferred)
- **Blockers:**
  - Problem 1: Scaler format mismatch
  - Problem 2: Output denormalization ambiguous
  - Problem 3: Model specificity not validated
  - Problem 5: "Frozen" state criteria missing
  - Problem 6: Input validation missing
  - Problem 10: Contact point/normal format undefined
  - Problem 13: Units/coordinate system not specified

### **Spec 01: CMake Build Support and Superquadric Fix**
- **Status:** Reportedly complete (MEMORY.md) but task list not provided
- **Readiness:** 50% (needs verification)
- **Blockers:**
  - Problem 16: Completion status unclear

### **Spec 02: LibTorch Build Integration**
- **Status:** Draft, not ready
- **Readiness:** 30% (design assumes Spec 00 and Spec 01 are solid; both have gaps)
- **Blockers:**
  - Problem 1: Scaler format (parser must handle actual format, not spec'd format)
  - Problem 7: Model path resolution undefined
  - Problem 2: Denormalization formula in runtime code

### **Spec 03: Surrogate Shape Type**
- **Status:** Draft, not ready
- **Readiness:** 50% (architecture clear, but initialization details missing)
- **Blockers:**
  - Problem 8: Shape database initialization syntax not specified
  - Problem 14: Multi-shape support undefined
  - Problem 16: Depends on Spec 01 completion

### **Spec 04: Geometric Contact Models**
- **Status:** Draft, not ready
- **Readiness:** 30% (high-level design clear, implementation details and validation missing)
- **Blockers:**
  - Problem 3: Model specificity not validated
  - Problem 4: Physics boundary not enforced
  - Problem 2: Denormalization in contact force calculation
  - Problem 10: Contact point/normal format undefined
  - Problem 15: Surrogate-specific tests missing

### **Spec 05: CTest Testing Infrastructure**
- **Status:** Draft, not ready
- **Readiness:** 40% (generic framework clear, but surrogate-domain tests missing)
- **Blockers:**
  - Problem 9: Test tolerance mismatched to model accuracy
  - Problem 15: Surrogate-specific tests not specified

---

## Cross-Project Dependencies

**This LIGGGHTS project depends on Phase 3B of SS_models_training:**

| Deliverable | Location | Status | Used By |
|---|---|---|---|
| run_029 model.pt (Option 4) | `/Programs/SS_models_training/interactions/ss_cube-wall/output/run_029/model.pt` | ✅ Ready | Spec 02 (model loading), Spec 04 (inference) |
| run_030 model.pt (Option 1) | `/Programs/SS_models_training/interactions/ss_cube-wall/output/run_030/model.pt` | ✅ Ready | Fallback selection (Spec 02, 04) |
| scalers.dat (run_029) | `/Programs/SS_models_training/interactions/ss_cube-wall/output/run_029/scalers.dat` | ⚠️ Format issue | Spec 02 (parser), **Problem 1** |
| scalers.dat (run_030) | `/Programs/SS_models_training/interactions/ss_cube-wall/output/run_030/scalers.dat` | ⚠️ Format issue | Spec 02 (parser), **Problem 1** |
| Training code + metadata | `/Programs/SS_models_training/.../run_029/training_code.py, run.json` | ✅ Ready | Reference for denormalization formula, **Problem 2** |

---

## Recommendations for Resolution

### **Phase 1: Clarify and Freeze Spec 00 (1–2 days)**

1. **Lock scaler format** (Problem 1):
   - Accept the actual format from run_029/run_030 as canonical.
   - Document all fields (format, run_id, model_type, y_mean, y_scale, etc.).
   - Add version field; define versioning scheme.

2. **Specify denormalization formula** (Problem 2):
   - Reference the actual training code (`run_029/training_code.py`).
   - Write explicit pseudocode with clear operator precedence.
   - Document role of `eps` and how it's computed.

3. **Add model identity validation** (Problem 3):
   - Require `model_type` field in scalers.dat (already present in actual files).
   - Specify runtime check: compare `model_type` in scaler against atom `shape_key`.
   - Define error message if mismatch detected.

4. **Define input validation** (Problem 6):
   - Specify valid ranges for `z_unit` (derived from training data range).
   - Specify behavior for `qw < 0` (canonicalize with warning or silently).
   - Specify handling of out-of-range or NaN inputs.

5. **Specify contact point/normal format** (Problem 10):
   - Define coordinate systems (local/global).
   - Define orientation conventions (normal direction).
   - Document constraints (||n|| = 1?, etc.).

6. **Integrate units/coordinates** (Problem 13):
   - Document LIGGGHTS unit system expected.
   - Define `L_actual` computation from particle attributes.
   - Specify rescaling formula precisely.

### **Phase 2: Redesign Spec 02 (Model Loading) (1 day)**

1. Specify absolute vs. relative path resolution.
2. Define environment variables (LIBTORCH_DIR, SS_MODELS_DIR).
3. Document error handling for missing models.
4. Specify deployment/installation instructions.

### **Phase 3: Redesign Spec 03 (Shape Type) (1 day)**

1. Specify input file syntax for declaring shape names.
2. Document shape database initialization flow.
3. Define error handling for invalid names, duplicates, etc.

### **Phase 4: Redesign Spec 04 (Contact Models) (2 days)**

1. Add explicit checks: model identity, output validity (no NaN/Inf).
2. Specify hard boundary between geometric (surrogate) and physics (Hertz) code.
3. Document contact history transitions and energy/momentum validation.
4. Add error recovery and fallback logic.

### **Phase 5: Redesign Spec 05 (Testing) (2 days)**

1. Lower default tolerance to match model accuracy (MAE + margin).
2. Add surrogate-specific unit tests (quaternion, rescaling, denormalization).
3. Add integration tests that exercise full pipeline.
4. Add regression tests for contact force and energy conservation.

### **Phase 6: Verify Spec 01 and Encode Dependencies (1 day)**

1. Confirm CMake superquadric fix is in current code.
2. Add dependency declarations to task lists.
3. Document prerequisite order.

---

## Sign-Off Checklist (Before Implementation)

- [ ] Spec 00: Scaler format frozen (actual format documented)
- [ ] Spec 00: Output denormalization formula with clear precedence
- [ ] Spec 00: Model identity validation specified
- [ ] Spec 00: Input validation ranges defined
- [ ] Spec 00: Contact point/normal format specified
- [ ] Spec 00: Units and L_actual integration specified
- [ ] Spec 02: Model path resolution (abs/rel/env vars) specified
- [ ] Spec 03: Input file syntax for shape names specified
- [ ] Spec 04: Model identity checks explicit in code spec
- [ ] Spec 04: Physics/geometric boundary enforced in design
- [ ] Spec 04: Contact history transitions specified
- [ ] Spec 04: Error recovery and fallback specified
- [ ] Spec 05: Test tolerance matched to model accuracy
- [ ] Spec 05: Surrogate-specific unit and integration tests specified
- [ ] Spec 01: CMake superquadric fix verified complete
- [ ] All specs: Dependency order documented in tasks

---

## Document Version

- **Version:** 1.0
- **Date:** 2026-11-05
- **Status:** Final Review (ready for user discussion and sign-off)
- **Next:** Await user direction on resolution priority and approach.

