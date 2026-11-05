# Spec 00 — Surrogate Interface Contract: Tasks

**Status:** Approved (QA-resolved 2026-11-05)

## Critical Issues Resolved (2026-11-05)

- [x] **T0.1: Lock scaler format to actual deliverable (ss_model_record_v1)** — RESOLVED
  - Scaler format is now formally documented with all mandatory fields
  - Format versioning is explicit (enables future compatibility)
  - Matches actual run_029 and run_030 models from training repo
  - Done when: Requirements document specifies format with all fields and examples

- [x] **T0.2: Clarify denormalization formula with explicit operator precedence** — RESOLVED
  - Formula is now: `overlap_unit = 10^(log10_output) - eps_small`
  - Operator precedence explicit: power operation first, then subtract, then rescale
  - Pseudocode provided for C++ reference implementation
  - Done when: Design document includes explicit formula and C++ pseudocode

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
  - Extract z_unit range from training data: [-0.5, +2.0]
  - Store in scalers.dat for reference
  - Enables runtime warnings if query is outside range
  - Done when: Training repo provides range metadata

## Validation and Testing (Link to Spec 05)

- [ ] **T0.8: Unit test for denormalization round-trip** (Spec 05)
  - Test: denormalize(normalize(x)) ≈ x for all training outputs
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
  - Test: Denormalization formula is exact (no numerical error)
  - Test: Rescaling by L_actual is exact
  - Test: overlap_phys is correct for various L_actual values
  - Done when: Spec 05 validation tests cover full output pipeline
