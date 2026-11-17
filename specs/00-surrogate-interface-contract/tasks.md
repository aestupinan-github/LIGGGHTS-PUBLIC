# Spec 00 — Surrogate Interface Contract: Tasks

**Status:** Approved — revised 2026-11-17

## Critical Issues Resolved (2026-11-05)

- [x] **T0.1: Lock scaler format to actual deliverable (ss_model_record_v1)** — RESOLVED
  - Scaler format is now formally documented with all mandatory fields
  - Format versioning is explicit (enables future compatibility)
  - Matches actual run_029 and run_030 models from training repo
  - Done when: Requirements document specifies format with all fields and examples

- [x] **T0.2: Clarify denormalization formula** — RESOLVED, **CORRECTED 2026-11-17**
  - ~~`overlap_unit = 10^(log10_output) - eps_small`~~ (wrong model; see requirements revision note)
  - Correct (run_029 option4): `sd_unit = r_s*y_scale + y_mean - h_exact(q)`, `sd_phys = sd_unit*L_actual`
  - Done when: requirements R00.1–R00.5 and design match training_code.py and the model output (verified)

- [x] **T0.2b: Correct the repo scalers.dat and C++ pipeline to the run_029 contract** (2026-11-17)
  - `SS_models/ss-cube-walls/scalers.dat` replaced by the run_029 original (y_mean/y_scale)
  - `surrogate_interface.h`: `h_exact_cube`, `denormalize_signed_distance`; log10/eps code removed
  - `surrogate_contact.h`: float32 inputs, z_unit = centre distance / L, option4-only guard
  - Done when: C++ path reproduces the Python reference (z=0.49/0.50/0.51 → -0.010033/-0.000069/+0.009911) ✓

- [x] **T0.3: Specify model identity validation at runtime** — RESOLVED
  - Model type (option4 vs option1) is now encoded in scalers.dat
  - Runtime validation is now required: particle shape_key must match model type
  - Error handling specified: fail-fast with diagnostic message
  - Done when: Requirements R00.8 specifies validation and error handling

- [x] **T0.4: Enforce consistency constraint (single geometric source)** — RESOLVED
  - Consistency constraint is now explicit: R00.9 Hard Constraint
  - Code pattern specified: query surrogate once, use all outputs from same call
  - Design section includes guard example code
  - Done when: Requirements and Design both reference consistency principle

## High-Priority Enhancements (QA-identified, deferred to Phase 3D)

- [ ] **T0.5: Define contact point and normal output format**
  - Needed for: ss-ss contact (Phase 3D)
  - Coordinate system convention (local/global)
  - Normal orientation convention
  - Constraints (||n|| = 1?, etc.)
  - Done when: Specs for ss-ss contact are finalized

- [ ] **T0.6: Implement model checksums and versioning** (Optional)
  - Add model_sha256 to scalers.dat (optional field)
  - Add model format version detection
  - Enable upgrade path if format changes
  - Done when: Model artifact management is formalized (Phase 4+)

- [ ] **T0.7: Document input feature ranges in scalers.dat** (Optional)
  - Extract z_unit range from training data: [0.400, 0.966] (dataset_003)
  - Store in scalers.dat for reference
  - Enables runtime warnings if query is outside range
  - Done when: Training repo provides range metadata

## Validation and Testing (Link to Spec 05)

- [ ] **T0.8: Unit test for denormalization against the training reference** (Spec 05)
  - Test: identity quaternion, z ∈ {0.49, 0.50, 0.51} → sd ≈ z - 0.5 (|err| < 2e-4); 45° about x at z=0.7071 → |sd| < 2e-4
  - Done when: Spec 05 testing infrastructure includes this unit test

- [ ] **T0.9: Unit test for quaternion canonicalization** (Spec 05)
  - Test: q and -q both map to same canonical form
  - Test: symmetric behavior (deterministic)
  - Done when: Spec 05 testing infrastructure includes this unit test

- [ ] **T0.10: Integration test for model loading and inference** (Spec 05)
  - Test: Load run_029 (option4) and verify model_type detection
  - Test: Load run_030 (option1) and verify model_type detection
  - Test: Query with test inputs; verify output ranges
  - Done when: Spec 05 testing framework includes these integration tests

- [ ] **T0.11: Validate input preparation pipeline** (Spec 05)
  - Test: z_unit computation is correct
  - Test: Quaternion canonicalization is applied
  - Test: Input tensor formation and type conversion
  - Done when: Spec 05 validation tests cover full input pipeline

- [ ] **T0.12: Validate output rescaling pipeline** (Spec 05)
  - Test: h_exact_cube matches compute_h_exact_batch for random quaternions
  - Test: Rescaling by L_actual is exact: sd_phys(L, z) = L · sd_unit(z/L)
  - Done when: Spec 05 validation tests cover full output pipeline
