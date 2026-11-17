# Spec 00 — Surrogate Interface Contract: Design

**Status:** Approved (QA-resolved 2026-11-05)

## Tensor Layout

> Revised 2026-11-17: the previous log10/eps design was wrong for run_029 (see requirements.md, revision note).

### Input (model signature `forward(position_z[B], quat[B,4])`, float32)
```
position_z = z_unit = centre_distance / L_actual
quat       = [qw, qx, qy, qz]   (canonical qw >= 0, unit norm)
```
- Training range: z_unit ∈ [0.400, 0.966]
- The model normalizes the quaternion again internally (`quat / (||quat|| + 1e-12)`).

### Output (current: 1 scalar)
```
r_s = StandardScaler-scaled residual,  residual = sd + h_exact(q)
```

**Denormalization (run_029, option4):**
```
sd_unit = (r_s * y_scale + y_mean) - h_exact(q)
sd_phys = sd_unit * L_actual
```
with `h_exact(q) = 0.5 (|u_x|+|u_y|+|u_z|)`, `u = R(q)^T d`, `d` = unit vector centre → wall.
For a floor `d = -e_z` this equals the training formula `max_v v·(-R^T e_z)`.

**Reference Implementation (from SS_models_training Phase 3B):**
`run_029/training_code.py`, `compute_h_exact_batch` (lines 66-114) and lines 365-371.

### Output (future: extendable to 7 scalars)
```
[sd, contact_point_x, contact_point_y, contact_point_z, normal_x, normal_y, normal_z]
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
y_mean 0.74250850860714912
y_scale 0.13142255689813576
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
- `input_dtype`: training-side value; the network weights are float32, so the C++ side feeds float32
- `input_units`: `canonical_L1` means L=1 training scale
- `y_mean`, `y_scale`: StandardScaler mean/scale of the residual target (NOT identity)
- `r_circ`: Circumradius constant for shape (0.866 for unit cube)
- `z_prefilter`: z_unit above which no contact is possible; the model is not called
- `sd_too_deep`: sd_unit below which the query is outside the training data (warning)
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
sd = (network(z, q) * y_scale + y_mean) - h_exact(q)

where:
  z = cube centre distance to wall / L_actual
  h_exact(q) = analytical support height (cube-specific)
  network(z, q) = scaled residual (training target sd + h_exact)
```
**Accuracy:** 84.7% @ 1e-4, MAE 6.1e-5 (run_029)

**Inference:**
1. Compute h_exact(q) analytically (rotation matrix method)
2. Call network forward pass with [z, q]
3. Combine: sd = (raw * y_scale + y_mean) - h_exact(q)

### Option 1: Pure Overlap Network
```
sd = network(z, q)

where:
  network = end-to-end learned model
  Inputs: [z, q]
  Output: signed distance directly
```
**Accuracy:** 55.3% @ 1e-4, MAE 7.6e-4 (run_030, fallback model)

**Inference:** not implemented in LIGGGHTS (model_type option1 is rejected at load time).

---

## Input Preparation Pipeline

1. **Particle State**: centre distance to wall (from the primitive wall: `deltan + radius`), quaternion, L_actual
2. **Canonicalize Quaternion**: if qw < 0, flip all components; normalize
3. **Nondimensionalize**: z_unit = centre_distance / L_actual
4. **Prefilter**: skip the model if z_unit > z_prefilter
5. **Form Inputs**: position_z = [z_unit], quat = [[qw, qx, qy, qz]], float32
6. **Validate**: check for NaN/Inf

---

## Output Processing Pipeline

1. **Model Inference**: forward pass → r_s
2. **Exact part**: h = h_exact(q, d)
3. **Denormalize**: sd_unit = r_s * y_scale + y_mean - h (R00.3)
4. **Rescale**: sd_phys = sd_unit * L_actual
5. **Validate**: NaN/Inf → error; sd_unit < sd_too_deep → warning
6. **Physics Integration**: deltan = sd_phys; contact if deltan <= 0

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
torch::Tensor pz   = torch::tensor({(float)z_unit}, torch::kFloat32);
torch::Tensor quat = torch::tensor({qw, qx, qy, qz}, torch::kFloat32).reshape({1, 4});
double raw     = model.forward({pz, quat}).toTensor().reshape({-1})[0].item<double>();
double h       = h_exact_cube(q, dir_wall);
double sd_phys = ((raw * y_scale + y_mean) - h) * L_actual;
```

---

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Input format | [z_unit, qw, qx, qy, qz] | Minimal, sufficient for wall contact; extensible for ss-ss |
| Output format | Single scalar (scaled residual) | Matches run_029 training target `sd + h_exact` |
| Denormalization | r_s*y_scale + y_mean - h_exact | Exact inverse of the training StandardScaler + residual decomposition |
| Rescaling | sd_unit * L_actual | Exact; unit cube is the nondimensional reference |
| dtype | float32 | Network weights are float32 |
| Quaternion flip | Silent (no warning) | Standard normalization; deterministic; no user awareness needed |
| Model identity | In scaler file (model_type) | Enables runtime model detection without filename parsing |
| Format versioning | Explicit (ss_model_record_v1) | Enables forward/backward compatibility if format changes |
| Error on mismatch | Yes (fail-fast) | Prevents silent garbage; early detection |

---

## Testing and Validation

**Unit Tests:**
- Denormalization against training reference: identity quaternion, z ∈ {0.49, 0.50, 0.51} → sd ≈ z - 0.5 (|err| < 2e-4)
- Quaternion canonicalization symmetry: q and -q both map to same canonical form
- Rescaling correctness: overlap(L=1) and overlap(L=2) agree with model behavior

**Integration Tests:**
- Load run_029 and run_030 models; verify model_type detection
- Query with test inputs; verify output ranges and no NaN/Inf
- Verify input/output tensor shapes

**Numerical Tests:**
- z_unit > z_prefilter: model not called; sd_unit < sd_too_deep: warning issued
- Very small/large L_actual: rescaling numerically stable
- Quaternions near boundaries (qw ≈ 0, qw ≈ 1): canonicalization correct
