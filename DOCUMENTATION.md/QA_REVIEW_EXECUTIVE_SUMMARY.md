# Phase 3C Specification Review — Executive Summary

**Date:** 2026-11-05  
**Scope:** LIGGGHTS Phase 3C (Surrogate Shape Integration) readiness assessment  
**Cross-Project Review:** Linkage between SS_models_training Phase 3B deliverables and LIGGGHTS Phase 3C specifications

---

## STATUS: ❌ NOT READY FOR IMPLEMENTATION

**Specifications Status:** Draft, with 21 significant issues identified  
**Blocking Issues:** 4 CRITICAL + 9 HIGH + 8 MEDIUM  
**Estimated Time to Fix:** 1–2 weeks (spec revision + validation)

---

## The Problem

LIGGGHTS Phase 3C specifications (Specs 00–05) define how to integrate trained ML surrogates (from SS_models_training Phase 3B) into LIGGGHTS for contact calculation. However, the specifications contain structural gaps and ambiguities that will lead to:

- **Silent model mismatches** at runtime (wrong shape geometry queried with wrong model)
- **Inconsistent implementations** (different LIGGGHTS versions interpret ambiguous specs differently)
- **Physical errors** (contact forces from geometrically inconsistent sources, replicating known bugs)
- **Deployment failures** (path resolution, version compatibility, error handling undefined)

---

## Critical Issues (4)

These must be resolved before any implementation begins:

### 1. **Scaler Format Mismatch**
- **Spec 00 says:** Simple key-value format (y_mean, y_scale, eps on separate lines)
- **Actual deliverable:** Extended format with version metadata and model type
- **Impact:** Parsers in LIGGGHTS will fail on real models
- **Fix:** Update Spec 00 to document actual format

### 2. **Output Denormalization Formula Ambiguous**
- **Spec 00 says:** `overlap_unit = 10^(output * y_scale + y_mean) - eps`
- **Problem:** Operator precedence unclear; different implementations will denormalize differently
- **Impact:** Contact forces computed from different overlap values (unphysical)
- **Fix:** Specify formula with explicit parentheses; reference actual training code

### 3. **Model Specificity Not Validated**
- **Problem:** Input tensor `[z_unit, qw, qx, qy, qz]` does not include shape identity
- **Risk:** User can query cube model with pyramid geometry; produces garbage silently
- **Impact:** Invalid contact geometry, unphysical forces, simulation divergence
- **Fix:** Add runtime validation that atom shape_key matches model type

### 4. **Contact Physics Boundary Not Enforced**
- **Problem:** No specification for preventing code from mixing surrogate overlap with natively-computed contact point
- **Risk:** Replicates known bug from prior discarded work (energy gain, non-physical bounces)
- **Impact:** Unphysical contact behavior, unstable simulations
- **Fix:** Specify explicit checks and boundaries in Spec 04 design

---

## High-Priority Structural Issues (9)

These require redesign before implementation:

5. **Scaler "Frozen" State Has No Objective Criteria** — "frozen" is vague; no versioning scheme
6. **Input Feature Validation Missing** — Ranges, edge cases, error modes not specified
7. **Model Path Resolution Undefined** — Absolute vs. relative paths, environment variables not clear
8. **Shape Database Initialization Syntax Not Specified** — Input file format for declaring shapes missing
9. **Test Tolerance Mismatched to Model Accuracy** — Default 1e-6 is 16× tighter than MAE 6.1e-5

10. **Contact Point/Normal Output Format Undefined** — Needed for future ss-ss contact
11. **Model Versioning and Compatibility Not Specified** — No upgrade path if format changes
12. **Error Recovery and Fallback Logic Not Specified** — Cannot degrade to fallback model at runtime
13. **Units/Coordinate System Integration Not Specified** — Silent unit mismatches possible

---

## Cross-Project Dependencies

**This project (SS_models_training) delivers:**

| Artifact | Location | Issue Category |
|---|---|---|
| run_029 model.pt (Option 4) | `interactions/ss_cube-wall/output/run_029/` | ✅ Ready |
| run_030 model.pt (Option 1) | `interactions/ss_cube-wall/output/run_030/` | ✅ Ready |
| scalers.dat (extended format) | Both run folders | ⚠️ Format mismatch (Critical Issue #1) |
| Training code + metadata | `training_code.py`, `run.json` | ⚠️ Denormalization unclear (Critical Issue #2) |
| Input ranges (implicit) | dataset_003 (100k samples) | ⚠️ Not documented (High Issue #6) |
| Quaternion canonicalization | CPP_test C++ code | ✅ Implemented |

**6 Issues in This Project:**
1. Scaler format: Delivered extended format; Spec 00 must be updated ← Critical Issue #1
2. Denormalization: Implemented correctly; needs documentation ← Critical Issue #2
3. Input ranges: Training data covers implicit range; needs formal spec ← High Issue #6
4. Model identity: Scalers.dat includes `model_type` field ✅ (solves Critical Issue #3)
5. L_actual definition: Training at L=1; LIGGGHTS must define ← High Issue #13
6. Quaternion canonicalization: Implemented; optional unit test for validation ← Minor

---

## Documentation Deliverables

### **LIGGGHTS Project**

📍 **Full QA Review:**  
`/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/QA_REVIEW_PHASE3C_SPECS_20261105.md`

- 21 detailed issues with severity levels
- Sign-off checklist for all specifications
- Dependency and order information
- Cross-links to training repo artifacts

📍 **Updated MEMORY.md:**  
`/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/MEMORY.md` (lines 76–97)

- QA review status and links
- Cross-project issue tracking
- Next steps and blocker status

---

### **SS_models_training Project**

📍 **Cross-Project Integration Issues:**  
`/home/su_estupinan@private.list.lu/Programs/SS_models_training/PHASE_3C_INTEGRATION_ISSUES_20261105.md`

- 6 issues originating from or concerning training repo
- Impact analysis
- Recommended actions for documentation and validation

📍 **Updated MEMORY.md:**  
`/home/su_estupinan@private.list.lu/Programs/SS_models_training/MEMORY.md` (lines 61–90)

- QA review summary
- Links to both review documents
- Action items (documentation, optional validation)

---

## Recommended Resolution Path

### **Phase 1: Clarify Specs 00–02 (1–2 days)**

Finalize interface contract based on **actual** deliverables from SS_models_training:

- [ ] Lock scaler format (accept extended format from run_029/run_030)
- [ ] Document denormalization formula with explicit pseudocode
- [ ] Specify model identity validation (use `model_type` field already present)
- [ ] Define input feature ranges (extract from dataset_003)
- [ ] Specify model path resolution and deployment
- [ ] Document L_actual computation and units integration

### **Phase 2: Revise Specs 03–04 (1 day)**

Update shape type and contact model specs:

- [ ] Specify shape database initialization (input file syntax)
- [ ] Add explicit validation checks (model identity, output validity)
- [ ] Enforce physics/geometry boundary with code guards
- [ ] Specify contact history transitions and error recovery

### **Phase 3: Revise Spec 05 (1 day)**

Update testing spec:

- [ ] Lower tolerance to match model accuracy (MAE + margin)
- [ ] Add surrogate-specific unit tests (quaternion, rescaling, denormalization)
- [ ] Add integration tests (full pipeline, error conditions)
- [ ] Add regression tests (contact force, energy conservation)

### **Phase 4: Validate Specs and Proceed to Design (1 day)**

- [ ] Confirm all 21 issues resolved
- [ ] LIGGGHTS team: Review updated specs and sign off
- [ ] SS_models_training team: Provide reference code/documentation
- [ ] Begin detailed design and task planning

---

## How to Use This Assessment

**For LIGGGHTS Team:**
1. Read `/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/QA_REVIEW_PHASE3C_SPECS_20261105.md` (full review)
2. Use the sign-off checklist to guide spec revisions
3. Reference the training repo artifacts (linked in review)
4. Update MEMORY.md as issues are resolved

**For SS_models_training Team:**
1. Read `PHASE_3C_INTEGRATION_ISSUES_20261105.md` (this directory)
2. Complete documentation action items in "Priority 1" section
3. Optional: Add validation tests from "Priority 2" section
4. Ensure training repo deliverables remain frozen until LIGGGHTS integration complete

**For Project Management:**
1. Schedule 1–2 weeks for spec clarification and revision
2. Confirm both teams understand interdependencies (see Cross-Project Dependencies table)
3. Do not start implementation until all 21 issues are resolved and sign-off checklist is complete
4. Maintain cross-project traceability (issues linked in both repos)

---

## Key Takeaways

✅ **What is Working:**
- Models are trained and exported (run_029, run_030)
- C++ inference pipeline is built and tested (Phase 3B)
- Scaler format is practical and includes necessary metadata
- Quaternion canonicalization is correctly implemented
- Model identity field (`model_type`) already present for runtime selection

❌ **What is Not Working:**
- Spec 00–05 are underspecified and partially contradictory
- No validation that inputs match model training domain
- No enforcement of physics/geometry boundary (risk of replicating prior bugs)
- No error recovery or fallback logic
- No clear deployment and path resolution strategy
- Test tolerances do not match model accuracy

🔄 **Next Steps:**
1. Resolve 4 critical issues (formal decision, not negotiable)
2. Redesign 9 high-priority specs (structural changes)
3. Update 8 missing specifications (complete domain coverage)
4. Sign off and proceed to detailed design
5. Both projects coordinate on versioning and deployment

---

## Document Locations

| Document | Location | Purpose |
|---|---|---|
| **Full LIGGGHTS QA Review** | `/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/QA_REVIEW_PHASE3C_SPECS_20261105.md` | Detailed 21-issue assessment with sign-off checklist |
| **Cross-Project Issues** | `/home/su_estupinan@private.list.lu/Programs/SS_models_training/PHASE_3C_INTEGRATION_ISSUES_20261105.md` | 6 issues in training repo + action items |
| **LIGGGHTS MEMORY Update** | `/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/MEMORY.md` (lines 76–97) | QA summary and status |
| **SS_models MEMORY Update** | `/home/su_estupinan@private.list.lu/Programs/SS_models_training/MEMORY.md` (lines 61–90) | Phase 3C QA section with action items |

---

**Status:** Ready for user review and team discussion.

