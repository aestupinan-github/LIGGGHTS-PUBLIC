# Spec 00 — Surrogate Interface Contract

**Status:** Approved — revised 2026-11-17 (contract corrected against the run_029 training code)

> **Revision 2026-11-17.** The previous version of R00.1–R00.5 described a log10 overlap
> model (`overlap = 10^out - eps`, `y_mean=0`, `y_scale=1`, z range [-0.5, 2.0]). That
> contract belongs to an older, unrelated model (`Cases/overlap_model.pt`) and is **wrong for
> run_029**. The contract below was verified against
> `SS_models_training/interactions/ss_cube-wall/output/run_029/training_code.py`
> (lines 200-223, 365-371), its `scalers.dat`, dataset_003, and by evaluating `model.pt`
> directly (see "Verification evidence" at the end). It matches the training team's
> `HANDOFF_TO_LIGGGHTS_C++_CORRECTIONS_20261105.md`.

## Context and Objective

Define the fixed interface contract between trained ML surrogate models (from SS_models_training Phase 3B) and LIGGGHTS runtime contact calculation. The surrogate is the single source of truth for geometric contact quantities (overlap, contact point, normal); physics models (Hertz, Hooke, etc.) consume surrogate output unchanged.

**Nondimensional principle.** The model is trained on a canonical cube of edge L=1. This
size is a *unit* (nondimensional) case, not a physical size: every particle is mapped onto
the unit cube by dividing lengths by its own edge length `L_actual`, the model is evaluated
in unit space, and the predicted signed distance is scaled back by `L_actual`.

## Requirements

### R00.1: Input Tensor Format

- **Model signature:** `forward(position_z: Tensor[B], quat: Tensor[B,4]) -> Tensor[B,1]`
  (two arguments; the model normalizes `quat` internally and concatenates to 5 features).
- **dtype:** float32 (the network weights are float32; float64 inputs are rejected by the model).
- `z_unit` (= `position_z`): distance from the **cube centre** to the wall plane, divided by
  `L_actual`. It is *not* a gap and *not* a penetration depth.
  - Training range (dataset_003, 100 000 rows): `z_unit ∈ [0.400, 0.966]`.
  - WHEN `z_unit > z_prefilter` (scalers.dat), THE SYSTEM SHALL skip the model: no contact is
    possible (`z_prefilter ≥ r_circ + gap margin`).
- `qw, qx, qy, qz`: particle orientation, body-to-world, `[qw, qx, qy, qz]` order.
  - IF input has qw < 0, THE SYSTEM SHALL flip all components (silently, no warning required)
  - THE SYSTEM SHALL normalize the quaternion to unit norm before use.

### R00.2: Output Tensor Format (Current: Signed Distance Only)

- **Shape:** single scalar `r_s` = StandardScaler-scaled residual, float32.
- Training target: `residual = sd + h_exact(q)` (training_code.py line 217), scaled with a
  StandardScaler whose mean/scale are stored as `y_mean` / `y_scale` in scalers.dat.
- There is **no log10 and no eps** in this model.
- WHEN surrogate outputs NaN or Inf, THE SYSTEM SHALL error with diagnostic message and refuse to compute contact

### R00.3: Output Denormalization Formula (option4)

```
residual = r_s * y_scale + y_mean
sd_unit  = residual - h_exact(q)

h_exact(q) = max over the 8 vertices v of the unit cube (half edge 0.5) of  v · (R(q)^T d)
  R(q) = body-to-world rotation matrix of q
  d    = unit vector from the particle centre toward the wall (floor: d = -e_z)
  (closed form for a cube: h = 0.5 (|u_x| + |u_y| + |u_z|),  u = R^T d)
  range: 0.5 (face toward wall) ... sqrt(3)/2 (vertex toward wall)
```

Sign convention (same as LIGGGHTS `deltan`): `sd > 0` gap, `sd < 0` penetration.

Source: training_code.py lines 368-371 (`residual_pred = scaler.inverse_transform(...)`,
`sd_pred = residual_pred - h_exact_val`) and `compute_h_exact_batch` (lines 66-114).

**Pseudocode (C++ Reference):**
```cpp
double h       = h_exact_cube(q, dir_wall);
double sd_unit = (raw * y_scale + y_mean) - h;
```

### R00.4: Physical Signed Distance

```
sd_phys = sd_unit * L_actual          (> 0 gap, <= 0 contact; penetration depth = -sd_phys)
```

### R00.5: Nondimensionalization and Length Scale

**Hard Constraint** (from CONSTITUTION.md, rule 9):

- **Training:** canonical unit cube, edge L=1 (nondimensional reference case).
- **Query Time:** rescale by the particle's own characteristic length `L_actual`
  - `z_unit  = centre_distance / L_actual` (input)
  - `sd_phys = sd_unit * L_actual` (output)
  - The quaternion and `h_exact` are scale-free.

**Definition of L_actual (CRITICAL):**
- **For ss_cube:** `L_actual` = cube edge length = particle diameter set with `set ... diameter`
  (Spec 03, R03.3). The particle radius is `L_actual/2` (inscribed sphere).
- **For other shapes** (future): L_actual = canonical size as defined at training time (documented in model metadata)
- **Constraint**: L_actual > 0. IF L_actual ≤ 0, THE SYSTEM SHALL error with diagnostic.

**Example (ss_cube, face toward wall, h = 0.5):**
```
L_actual = 0.01 m, centre 4.9 mm above the wall
  => z_unit  = 0.0049 / 0.01 = 0.49
  => model + h_exact give sd_unit = -0.01   (identical to an L=1 cube at z = 0.49 m)
  => sd_phys = -0.01 * 0.01 = -1.0e-4 m     (0.1 mm penetration)
```

### R00.5b: Validity Range

- WHEN `sd_unit < sd_too_deep` (scalers.dat, -0.1 for run_029), THE SYSTEM SHALL warn
  (penetration deeper than the training data; the model extrapolates) and SHALL proceed.

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
input_dtype float64              # training-side value; the C++ side feeds float32 (R00.1)
input_units canonical_L1
y_mean <float>                   # StandardScaler mean of residual (run_029: 0.74250850860714912)
y_scale <float>                  # StandardScaler scale of residual (run_029: 0.13142255689813576)
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
- `model_type option4`: Physics-informed residual (`sd = r_s*y_scale + y_mean - h_exact`, R00.3)
- `model_type option1`: Pure overlap network (run_030) — contract **not implemented**
- WHEN model_type is not `option4`, THE SYSTEM SHALL error with diagnostic (until option1 is specified and implemented)

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
| **sd_unit** | Signed distance cube–wall for the unit cube (>0 gap, <0 penetration) |
| **sd_phys** | Physical signed distance = sd_unit · L_actual; used as LIGGGHTS `deltan` |
| **z_unit** | Cube centre distance to the wall / L_actual (model input `position_z`) |
| **h_exact(q)** | Exact support height of the unit cube toward the wall (depth of the lowest vertex below the centre) |
| **r_s** | Raw network output: StandardScaler-scaled residual `sd + h_exact` |
| **L_actual** | Particle's characteristic length (edge length for cube = particle diameter) |
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
- RF-5: THE SYSTEM SHALL nondimensionalize the input by L_actual and rescale sd_unit by L_actual.
- RF-6: WHEN surrogate output is NaN/Inf or outside expected range, THE SYSTEM SHALL error with diagnostic.
- RF-7: THE SYSTEM SHALL ensure all geometric outputs (overlap, contact point, normal) come from single surrogate call.
- RF-8: THE SYSTEM SHALL validate model identity (shape_key vs model type) before computing contact.

---

## Numerical and Physical Requirements

- NR-1: Input range (z_unit): [0.400, 0.966] (dataset_003); sd range [-0.09998, 0.46585]
- NR-2: Accuracy zone sd_unit ∈ [-0.1, 0.1]; run_029 overlap MAE 6.1e-5 (84.7 % within 1e-4)
- NR-3: Denormalization is affine (no underflow/overflow possible)
- NR-4: Quaternion canonicalization must be exact (deterministic, reversible)
- NR-5: Length rescaling must be exact: `sd_phys(L, z) = L · sd_unit(z/L)` (no tolerance)
- NR-6: Model identity validation must be fail-fast (error immediately if mismatch detected)

---

## Non-Functional Requirements

- NFR-1: Interface must be portable across different LIGGGHTS builds (CMake, Makefile)
- NFR-2: Scaler parsing must be fast (< 1ms per model load)
- NFR-3: Runtime validation must add negligible overhead (< 1% per contact calculation)
- NFR-4: Error messages must be diagnostic and actionable (not cryptic or silent)

---

## Edge Cases

- EC-1: WHEN z_unit > z_prefilter the model SHALL NOT be called; WHEN sd_unit < sd_too_deep THE SYSTEM SHALL warn (R00.5b)
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
- CC-2: Denormalization formula is explicit with clear pseudocode and traced to the training code
- CC-3: Input validation ranges are extracted from training data (dataset_003)
- CC-4: Model identity validation is specified and testable
- CC-5: All 4 critical issues (QA-identified) are resolved in this spec

---

## Verification evidence (2026-11-17)

- `SS_models/ss-cube-walls/model.pt` is byte-identical to `run_029/model.pt`; weights float32.
- Recomputed over dataset_003: `mean(sd+h) = 0.742508508`, `std(sd+h) = 0.131422556`
  (= run_029 scalers.dat `y_mean`, `y_scale`); `max|sd - (z - h)| = 3.4e-7`.
- `model.pt` with the R00.3 formula, identity quaternion (truth `sd = z - 0.5`):
  z=0.49 → -0.010033, z=0.50 → -0.000069, z=0.51 → +0.009911, z=0.60 → +0.100013.
  45° about x (h = 0.70711): z=0.7071 → -0.000030 (truth -0.000007).
  3000 random dataset rows: MAE 6.0e-5.
- The same numbers are reproduced by the C++ path (`h_exact_cube` + LibTorch, float32).
