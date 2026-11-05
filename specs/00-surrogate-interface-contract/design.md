# Spec 00 — Surrogate Interface Contract: Design

**Status:** Approved (QA-resolved 2026-11-05)

## Tensor Layout

### Input (5 features)
```
[z_unit, qw, qx, qy, qz]
```
- `z_unit = z_phys / L_actual` (nondimensionalized height)
  - Computed at input preparation time
  - Range: [-0.5, +2.0] (from training data dataset_003)
- Quaternion canonicalized: if qw < 0, flip all components
  - Applied deterministically before model query
  - Ensures model sees consistent orientation domain

### Output (current: 1 scalar)
```
log10(overlap_unit)
```

**Denormalization (CRITICAL - Fixed Formula):**

```
overlap_unit = 10^(log10_overlap) - eps_small

where:
  log10_overlap = raw model output (in log10 domain)
  eps_small ≈ 1e-15 (prevents underflow in log domain)
```

**For current models (run_029, run_030) with y_mean=0.0, y_scale=1.0:**

The formula simplifies to:
```cpp
double overlap_unit = std::pow(10.0, log10_overlap) - 1e-15;
```

**Physical overlap:**
```
overlap_phys = overlap_unit * L_actual
```
where L_actual is the particle's characteristic length (edge length for ss_cube).

**Operator Precedence (Explicit):**
- `10^(log10_overlap)` is evaluated first (power function)
- Then subtract `eps_small` from the result
- Then multiply by `L_actual`

**Derivation:**
- Training: overlaps in physical space, transformed to log10 scale for neural network regression
- Model learns: f(z, q) ≈ log10(overlap_unit)
- Inverse: To recover overlap_unit, exponentiate: 10^f(z, q) ≈ overlap_unit
- Eps correction: Small offset to handle numerical edge cases (log10 underflow)

**Reference Implementation (from SS_models_training Phase 3B):**
See `/home/su_estupinan@private.list.lu/Programs/SS_models_training/interactions/ss_cube-wall/output/run_029/training_code.py` for exact training-time inverse transform.

### Output (future: extendable to 7 scalars)
```
[log10(overlap_unit), contact_point_x, contact_point_y, contact_point_z, normal_x, normal_y, normal_z]
```
Required for ss-ss and non-primitive wall contact. Format frozen in Spec 00 once contact point generation is complete (deferred to Phase 3D).

---

## Scaler Format (Frozen)

**File:** `SS_models/<shape>-<object>/scalers.dat` (text, ASCII key-value)

**Format Version:** `ss_model_record_v1` (explicit versioning for backward compatibility)

**Example (run_029 - Option 4, Physics-Informed Residual):**
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

**Field Meanings:**
- `format`: Version identifier (enables future format changes)
- `run_id`: Unique identifier for training run (for reproducibility)
- `model_type`: `option4` (physics-informed) or `option1` (pure ANN)
- `input_dtype`: Input feature precision (float64 in training; float32 in C++)
- `input_units`: `canonical_L1` means L=1 training scale
- `y_mean`, `y_scale`: Denormalization parameters (currently 0.0, 1.0 for both models)
- `r_circ`: Circumradius constant for shape (0.866 for unit cube)
- `z_prefilter`: Prefilter threshold for near-contact detection
- `sd_too_deep`: Penetration depth threshold beyond which contact is invalid
- `h_exact_method`: How h_exact(q) is computed (analytical_rotation_matrix for Option 4)
- `network_input_dim`: Number of input features (always 5: z_unit, qw, qx, qy, qz)
- `ref_eval`: Reference evaluation mode (CPU vs GPU; not used in LIGGGHTS)
- `ref_sd_tol`: Reference solution tolerance (for validation; not used in LIGGGHTS)
- `n_ref`: Number of stored reference test cases (0 if none)

**Backwards Compatibility:**
- Parsers MUST check `format` field first
- If format is unrecognized, error with diagnostic
- If format is v1, parser MUST extract all mandatory fields
- Unknown optional fields MUST be ignored (allows safe forward compatibility)

---

## Model Type Architecture

### Option 4: Physics-Informed Residual Learning
```
sd = z - h_exact(q) - network(z, q)

where:
  z = height above wall (nondimensionalized)
  h_exact(q) = analytical support function (cube-specific)
  network(z, q) = learned residual correction
```
**Accuracy:** 84.7% @ 1e-4, MAE 6.1e-5 (run_029)

**Inference:**
1. Compute h_exact(q) analytically (rotation matrix method)
2. Call network forward pass with [z, q]
3. Combine: sd = z - h_exact(q) - residual

### Option 1: Pure Overlap Network
```
sd = network(z, q)

where:
  network = end-to-end learned model
  Inputs: [z, q]
  Output: signed distance directly
```
**Accuracy:** 55.3% @ 1e-4, MAE 7.6e-4 (run_030, fallback model)

**Inference:**
1. Call network forward pass with [z, q]
2. Output is signed distance directly

---

## Input Preparation Pipeline

1. **Particle State**: z_phys, quaternion, L_actual
2. **Canonicalize Quaternion**: if qw < 0, flip all components
3. **Nondimensionalize Height**: z_unit = z_phys / L_actual
4. **Form Input Tensor**: [z_unit, qw, qx, qy, qz]
5. **Type Conversion**: convert float64 → float32 for model inference
6. **Validate**: check for NaN/Inf, ranges

---

## Output Processing Pipeline

1. **Model Inference**: forward pass → log10(overlap_unit) or residual
2. **Denormalize**: overlap_unit = 10^(output) - eps (R00.3)
3. **Rescale**: overlap_phys = overlap_unit * L_actual
4. **Validate**: check for NaN/Inf, physically invalid values (overlap > 0)
5. **Physics Integration**: pass overlap + contact_point + normal to physics model

---

## Consistency Guarantee

**Single Source of Truth:**
The surrogate model is the ONLY source of geometric contact quantities. Once the surrogate is queried:
- Overlap comes from surrogate denormalization
- Contact point comes from surrogate output (future)
- Contact normal comes from surrogate output (future)

**No Mixing:**
- Do NOT use surrogate overlap with native-computed contact point
- Do NOT use multiple surrogates for different outputs
- Do NOT post-hoc modify surrogate outputs (clamping, smoothing, etc.)

**Error Detection:**
Code review MUST verify that contact calculation follows this pattern:
```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag) {
    // Query surrogate ONCE
    auto surrogate_output = model->forward(input_tensor);
    
    // Extract ALL outputs from single surrogate call
    overlap = denormalize(surrogate_output);
    contact_point = ...; // future
    normal = ...; // future
    
    // Use ALL from same source
    force = physics_model(overlap, contact_point, normal);
}
#endif
```

---

## Model Wrapper Interface

### Header (C++ Public Interface)

```cpp
class SurrogateModel {
public:
    // Load model and scalers
    SurrogateModel(const std::string& model_path, const std::string& scaler_path);
    
    // Query model
    torch::Tensor forward(const torch::Tensor& z_unit, const torch::Tensor& quat);
    
    // Get model metadata
    std::string model_type() const;        // "option4" or "option1"
    std::string run_id() const;            // "run_029", "run_030", etc.
    int input_features() const;            // always 5
    
    // Get shape geometry (baked at training time)
    std::vector<Vec3> get_shape_points() const;
};
```

### Initialization (at LIGGGHTS init() time)

```cpp
SurrogateModel* model = new SurrogateModel(
    "SS_models/ss-cube-walls/model.pt",
    "SS_models/ss-cube-walls/scalers.dat"
);

// Verify model is for the right shape
if (model->model_type() != "option4" && model->model_type() != "option1") {
    error->all(FLERR, "Surrogate: unknown model_type");
}
```

### Runtime Query

```cpp
// Prepare input
torch::Tensor z_unit = torch::tensor({z_phys / L_actual}, at::kFloat32);
torch::Tensor quat = torch::tensor({qw, qx, qy, qz}, at::kFloat32);

// Query surrogate
torch::Tensor log10_overlap = model->forward(z_unit, quat);

// Denormalize and rescale
double overlap_unit = std::pow(10.0, log10_overlap.item<float>()) - 1e-15;
double overlap_phys = overlap_unit * L_actual;
```

---

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Input format | [z_unit, qw, qx, qy, qz] | Minimal, sufficient for wall contact; extensible for ss-ss |
| Output format | Single scalar (log10 scale) | Numerically stable; covers [-5, +0.5] range; easy to denormalize |
| Denormalization | 10^(output) - eps | Exact inverse of log10 transform; matches training procedure |
| Rescaling | overlap_unit * L_actual | Exact; no tolerance or approximation needed |
| Quaternion flip | Silent (no warning) | Standard normalization; deterministic; no user awareness needed |
| Model identity | In scaler file (model_type) | Enables runtime model detection without filename parsing |
| Format versioning | Explicit (ss_model_record_v1) | Enables forward/backward compatibility if format changes |
| Error on mismatch | Yes (fail-fast) | Prevents silent garbage; early detection |

---

## Testing and Validation

**Unit Tests:**
- Denormalization round-trip: denormalize(normalize(x)) ≈ x
- Quaternion canonicalization symmetry: q and -q both map to same canonical form
- Rescaling correctness: overlap(L=1) and overlap(L=2) agree with model behavior

**Integration Tests:**
- Load run_029 and run_030 models; verify model_type detection
- Query with test inputs; verify output ranges and no NaN/Inf
- Verify input/output tensor shapes

**Numerical Tests:**
- Extreme z_unit values (far outside training range): model still evaluates (extrapolation acceptable)
- Very small/large L_actual: rescaling numerically stable
- Quaternions near boundaries (qw ≈ 0, qw ≈ 1): canonicalization correct
