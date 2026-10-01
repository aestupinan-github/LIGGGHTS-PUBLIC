# Spec 00 — Surrogate Interface Contract

## Requirements

### Input Tensor
- **Shape**: `[z_unit, qw, qx, qy, qz]` (5 features, float32)
- `z_unit`: particle center height normalized by characteristic length (z_phys / L_actual)
- `qw, qx, qy, qz`: particle orientation quaternion, canonicalized to qw >= 0

### Output Tensor
- **Current**: single scalar `log10(overlap)` (normalized)
- **Future**: extendable to contact point + normal (for ss-ss and non-primitive walls)

### Nondimensionalization Convention
- Train once at canonical L=1
- At query time, rescale by particle's own characteristic length: `overlap_phys = overlap_unit * L_actual`
- This is a hard constraint — not open for debate

### Quaternion Canonicalization
- Enforce qw >= 0 (flip all components if qw < 0)
- This ensures the surrogate sees a consistent orientation representation

### Scaler File Format
- File: `scalers.dat` (text, key-value format)
- Contents:
  ```
  y_mean
  <value>
  y_scale
  <value>
  eps
  <value>
  ```
- `y_mean`, `y_scale`: scalars for denormalizing model output
- `eps`: small constant to avoid log(0) issues
- **Note**: Current scalers.dat files have 4 values per line (likely from a multi-output model with normals). The model is not frozen yet — this format must be revisited when the model is frozen.

### Consistency Constraint (HARD CONSTRAINT)
- Overlap AND contact point/normal MUST come from the same consistent geometric evaluation
- Do NOT mix a learned overlap with a natively-computed contact point/lever-arm derived from a different source
- This mismatch was identified as a likely cause of energy-gain/non-physical-bounce bugs in prior (discarded) work

### Model Specificity
- Each model is trained on a specific (shape, object) pair
- Example: `ss-cube-walls`, `ss-piramide-walls`, `ss-cube-ss-cube`
- Model selection at runtime via shape key + object type

### Model Wrapper
- Bakes in shape point cloud (generatrice points) at training time
- Can generate shape vertices for postprocessing without needing STL file at runtime
- More robust than loading STL at runtime
