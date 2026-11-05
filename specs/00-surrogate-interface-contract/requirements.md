# Spec 00 — Surrogate Interface Contract

**Status:** Approved (QA-resolved 2026-11-05)

## Context and Objective

Define the fixed interface contract between trained ML surrogate models (from SS_models_training Phase 3B) and LIGGGHTS runtime contact calculation. The surrogate is the single source of truth for geometric contact quantities (overlap, contact point, normal); physics models (Hertz, Hooke, etc.) consume surrogate output unchanged.

## Requirements

### R00.1: Input Tensor Format

- **Shape**: `[z_unit, qw, qx, qy, qz]` (5 features, float32)
- `z_unit`: particle center height normalized by characteristic length (z_phys / L_actual)
  - Derived from particle position relative to wall and particle's own length scale
  - Range: typically [-0.5, +2.0] for training data (from dataset_003, 100k samples)
  - WHEN `z_unit` is outside training range, THE SYSTEM SHALL either warn (log message) or clamp to nearest trained value (TBD by implementation)
- `qw, qx, qy, qz`: particle orientation quaternion (unit norm, canonicalized to qw >= 0)
  - IF input has qw < 0, THE SYSTEM SHALL flip all components (silently, no warning required)
  - WHEN quaternion is not unit-normalized (||q|| != 1), THE SYSTEM SHALL either normalize or error with diagnostic (TBD by implementation)

### R00.2: Output Tensor Format (Current: Overlap Only)

- **Shape**: Single scalar `log10(overlap_unit)` (normalized, float32)
- Output range: typically [-5.0, +0.5] (from training data; log10 scale)
- WHEN surrogate outputs NaN or Inf, THE SYSTEM SHALL error with diagnostic message and refuse to compute contact
- WHEN surrogate output is far outside training range (> 3σ from training distribution), THE SYSTEM MAY warn but SHALL proceed (acceptable for extrapolation)

### R00.3: Output Denormalization Formula

**Single Scalar Case (Current, Option 4 and Option 1):**

```
overlap_unit = 10^(output) - eps_small

where:
  output = raw model output (log10 scale)
  eps_small ≈ 1e-15 (prevents log domain underflow)
```

**Derivation and Rationale:**
- Training data: overlaps in physical space, denormalized to log10 scale for regression
- Model output: `log10(overlap_unit)` where `overlap_unit = overlap_phys / L_actual`
- Inverse: `overlap_unit = 10^(output) - eps`
- For y_mean=0.0, y_scale=1.0 (both models, run_029 and run_030): formula simplifies to `overlap_unit = 10^(output) - eps`

**Pseudocode (C++ Reference):**
```cpp
double denormalize_overlap(double log10_output, double y_mean, double y_scale, double eps) {
    // Reverse: exponentiate + remove eps
    return std::pow(10.0, (log10_output * y_scale + y_mean)) - eps;
}

// For current models (y_mean=0, y_scale=1, eps≈1e-15):
double overlap_unit = std::pow(10.0, log10_output) - 1e-15;
```

### R00.4: Physical Overlap Computation

```
overlap_phys = overlap_unit * L_actual

where:
  overlap_unit = denormalized model output (from R00.3)
  L_actual = particle's characteristic length (see R00.5)
  overlap_phys = physical overlap in simulation units (always ≤ 0 for actual contact)
```

### R00.5: Nondimensionalization and Length Scale

**Hard Constraint** (from CONSTITUTION.md, rule 9):

- **Training:** Models trained at canonical size L=1 (for ss_cube: edge length = 1.0)
- **Query Time:** Rescale by particle's own characteristic length L_actual
  - `z_unit = z_phys / L_actual` (input preparation)
  - `overlap_phys = overlap_unit * L_actual` (output rescaling)

**Definition of L_actual (CRITICAL):**
- **For ss_cube** (current target): L_actual = edge_length of cube (as declared in input file or per-particle attribute)
- **For other shapes** (future): L_actual = canonical size as defined at training time (documented in model metadata)
- **Constraint**: L_actual > 0. IF L_actual ≤ 0, THE SYSTEM SHALL error with diagnostic.
- **Numerical Stability**: IF two particles have L values differing by >1000× (e.g., 0.001 vs 1.0), THE SYSTEM MAY warn but SHALL proceed (rescaling is exact; no tolerance needed).

**Example (ss_cube):**
```
Training: cube edge_length = 1.0, particle at z_phys = 0.1, overlap = 0.01
Query: cube edge_length = 0.5, particle at z_phys = 0.05
  => z_unit = 0.05 / 0.5 = 0.1   (same as training)
  => model predicts same log10(overlap_unit) ≈ -2.0
  => overlap_unit ≈ 0.01
  => overlap_phys = 0.01 * 0.5 = 0.005 (physically correct for L_actual=0.5)
```

### R00.6: Quaternion Canonicalization

- **Constraint**: All quaternions must have qw >= 0
- **Canonicalization Rule**: IF qw < 0, flip all components: q = -q
- **When Applied**: At input preparation time, before passing to surrogate model
- **Logging**: Silent (no warning required; this is a standard normalization step)
- **Verification**: Canonicalization MUST be reversible and deterministic (same quaternion always canonicalizes to same value)

### R00.7: Scaler File Format (Frozen)

**File Location:** `SS_models/<shape>-<object>/scalers.dat` (text, ASCII key-value format)

**Mandatory Fields:**
```
format ss_model_record_v1
run_id <string>
model_type <string>              # MUST be "option4" or "option1"
input_dtype float64
input_units canonical_L1
y_mean <float>                   # denormalization intercept
y_scale <float>                  # denormalization scale
r_circ <float>                   # circumradius (for ss_cube: ~0.866)
z_prefilter <float>              # prefilter threshold
sd_too_deep <float>              # penetration threshold
h_exact_method <string>          # method for h_exact (e.g., "analytical_rotation_matrix")
network_input_dim <int>          # input feature count (must be 5)
ref_eval <string>                # reference evaluation mode
ref_sd_tol <float>               # reference tolerance
n_ref <int>                      # number of reference test cases (0 if none)
```

**Format Version:** `ss_model_record_v1` (versioning is explicit; future changes use v2, v3, etc.)

**Model Type Detection:**
- `model_type option4`: Physics-informed residual (sd = z - h_exact - network_residual)
- `model_type option1`: Pure overlap network (sd = network(z, q))
- WHEN model_type is unrecognized, THE SYSTEM SHALL error with diagnostic

**Optional Fields (for future use):**
```
n_ref <int>
ref_test_case_1 <string>
model_sha256 <string>            # model checksum (deferred)
```

**Source:** Confirmed format from `/home/su_estupinan@private.list.lu/Programs/SS_models_training/interactions/ss_cube-wall/output/run_029/scalers.dat` and run_030.

### R00.8: Model Specificity and Identity Validation

**Constraint**: Each model is trained on ONE specific (shape, object) pair; runtime must validate match.

**Model Identity:**
- Encoded in scalers.dat: `run_id`, `model_type`, `input_units`
- Particle identifies itself: shape_key (int) or shape_name (string)

**Validation (REQUIRED):**
- WHEN particle queries surrogate, THE SYSTEM SHALL verify that particle's shape_key matches loaded model type
- IF shape_key does NOT match (e.g., particle declared ss-cube but model is ss-pyramid), THE SYSTEM SHALL error with diagnostic: "Shape mismatch: particle shape_key=<N> does not match model <model_type> from run <run_id>"
- WHEN model is not found for requested shape_key, THE SYSTEM SHALL error with diagnostic: "No surrogate model loaded for shape_key=<N>"

**Rationale**: Silent model mismatches produce garbage contact geometry, unphysical forces, and simulation divergence. Early detection is essential.

### R00.9: Consistency Constraint (HARD CONSTRAINT)

**Principle**: Overlap AND contact point/normal MUST come from the same single geometric evaluation.

**Rule**: Do NOT mix:
- Learned overlap (from surrogate) + natively-computed contact point/normal (from geometric algorithm)
- Different surrogate models for different outputs
- Surrogate outputs with post-hoc modifications (e.g., clamping, smoothing)

**Implementation Guard**: Code that computes contact must follow this pattern:
```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag) {
    // 1. Query surrogate ONCE for all outputs
    surrogate_output = model->forward(z_unit, quat);
    
    // 2. Extract all outputs from single surrogate call
    overlap = denormalize(surrogate_output);
    contact_point = surrogate_output.contact_point;  // future
    normal = surrogate_output.normal;                // future
    
    // 3. Use surrogate outputs directly (no recomputation)
    force = physics_model(overlap, contact_point, normal);
}
#endif
```

**Consequence of Violation**: Prior (discarded) work mixed geometric sources and produced energy-gain, non-physical bounce bugs. This must not recur.

### R00.10: Error Handling and Edge Cases

**Invalid Inputs:**
- IF `z_unit` is NaN or Inf, THE SYSTEM SHALL error with diagnostic
- IF any quaternion component is NaN or Inf, THE SYSTEM SHALL error with diagnostic
- IF `L_actual` is NaN, Inf, or ≤ 0, THE SYSTEM SHALL error with diagnostic

**Invalid Outputs:**
- IF model outputs NaN or Inf, THE SYSTEM SHALL error with diagnostic: "Surrogate produced invalid output (NaN/Inf) at z_unit=<value>, quat=<values>"
- IF denormalized overlap is NaN or Inf after rescaling, THE SYSTEM SHALL error with diagnostic

**Error Messages Must Include:**
- What went wrong (NaN input, model failure, shape mismatch, etc.)
- Relevant values (z_unit, quat, shape_key, model run_id, etc.)
- Suggested action (check input, reload model, verify shape assignment, etc.)

---

## Definitions

| Term | Definition |
|------|-----------|
| **overlap_unit** | Normalized overlap in model training units; overlap relative to L=1 |
| **overlap_phys** | Physical overlap in simulation units; signed distance (≤0 for contact) |
| **z_unit** | Normalized height: z_phys / L_actual |
| **z_phys** | Physical height of particle above wall |
| **L_actual** | Particle's characteristic length (edge length for cube) |
| **log10(overlap)** | Log10-transformed overlap (model output domain) |
| **qw >= 0** | Quaternion canonicalization constraint (enforced at input time) |
| **ss_cube-walls** | Interaction type: surrogate cube vs wall (current target) |
| **option4** | Physics-informed residual model (run_029, 84.7% @ 1e-4) |
| **option1** | Pure overlap network (run_030, 55.3% @ 1e-4) |

---

## Functional Requirements

- RF-1: THE SYSTEM SHALL parse scalers.dat and extract model identity, denormalization constants, and input/output specifications.
- RF-2: THE SYSTEM SHALL detect and error if model type (option4 vs option1) does not match requested shape.
- RF-3: THE SYSTEM SHALL canonicalize quaternions (flip if qw < 0) before passing to surrogate.
- RF-4: THE SYSTEM SHALL denormalize model output using exact formula from R00.3.
- RF-5: THE SYSTEM SHALL rescale overlap by L_actual (input nondimensionalization, output rescaling).
- RF-6: WHEN surrogate output is NaN/Inf or outside expected range, THE SYSTEM SHALL error with diagnostic.
- RF-7: THE SYSTEM SHALL ensure all geometric outputs (overlap, contact point, normal) come from single surrogate call.
- RF-8: THE SYSTEM SHALL validate model identity (shape_key vs model type) before computing contact.

---

## Numerical and Physical Requirements

- NR-1: Input feature ranges (z_unit): [-0.5, +2.0] (from dataset_003 training data)
- NR-2: Output log10(overlap): [-5.0, +0.5] (typical range from trained models)
- NR-3: Denormalization must be numerically stable for all log10 values in [-5.0, +0.5] (no underflow/overflow)
- NR-4: Quaternion canonicalization must be exact (deterministic, reversible)
- NR-5: Length rescaling must preserve physical correctness: `overlap_phys(L) = overlap_unit(1.0) * L` (exact, no tolerance)
- NR-6: Model identity validation must be fail-fast (error immediately if mismatch detected)

---

## Non-Functional Requirements

- NFR-1: Interface must be portable across different LIGGGHTS builds (CMake, Makefile)
- NFR-2: Scaler parsing must be fast (< 1ms per model load)
- NFR-3: Runtime validation must add negligible overhead (< 1% per contact calculation)
- NFR-4: Error messages must be diagnostic and actionable (not cryptic or silent)

---

## Edge Cases

- EC-1: WHEN z_unit is far outside training range (|z_unit| > 10), THE SYSTEM MAY warn or clamp (acceptable for extrapolation)
- EC-2: WHEN particle size L_actual changes during simulation, rescaling must recompute for each step (correct by construction)
- EC-3: WHEN two particles have very different sizes (1000× difference), rescaling still works (no numerical issue)
- EC-4: WHEN model is not found at init time, error must include path searched and suggestions

---

## Out of Scope (Deferred)

- Contact point and normal output (future: ss-ss and non-primitive wall contact)
- Multiple shapes per simulation (future: Spec 03 milestone)
- Model versioning and checksums (deferred: Spec 02 enhancement)
- Per-particle model selection (deferred: Spec 03+ feature)

---

## Completion Criteria

- CC-1: Scaler format is formally documented and matches actual models (run_029, run_030)
- CC-2: Denormalization formula is explicit with clear pseudocode
- CC-3: Input validation ranges are extracted from training data
- CC-4: Model identity validation is specified and testable
- CC-5: All 4 critical issues (QA-identified) are resolved in this spec
