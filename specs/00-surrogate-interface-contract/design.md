# Spec 00 — Surrogate Interface Contract: Design

## Tensor Layout

### Input (5 features)
```
[z_unit, qw, qx, qy, qz]
```
- `z_unit = z_phys / L_actual` (nondimensionalized height)
- Quaternion canonicalized: if qw < 0, flip all components

### Output (current: 1 scalar)
```
log10(overlap_unit)
```
Denormalization: `overlap_unit = 10^(output * y_scale + y_mean) - eps`
Physical overlap: `overlap_phys = overlap_unit * L_actual`

### Output (future: extendable)
```
[log10(overlap_unit), contact_point_x, contact_point_y, contact_point_z, normal_x, normal_y, normal_z]
```
Required for ss-ss and non-primitive wall contact.

## Scaler Format

```
y_mean
<scalar>
y_scale
<scalar>
eps
<scalar>
```

Single-output model. Revisit when model is frozen.

## Consistency Guarantee

The surrogate model is the single source of truth for geometric contact quantities. The physics model (Hertz, Hooke, etc.) consumes surrogate output (overlap, contact point, normal) unchanged. No mixing of geometric sources.

## Model Wrapper Design

```
SS_models/<shape>-<object>/
  overlap_model.pt    (TorchScript model)
  scalers.dat         (normalization constants)
  shape_points.baked  (generatrice points for postprocessing, baked at training time)
```

The wrapper class:
1. Loads model + scalers at `init()`
2. Provides `predict(input_tensor) -> output_tensor` for contact calculation
3. Provides `get_shape_points() -> vector<vec3>` for postprocessing
