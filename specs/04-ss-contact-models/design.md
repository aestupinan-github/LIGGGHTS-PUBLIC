# Spec 04 — Geometric Contact Models: Design

## Architecture

```
Training time:
  STL / generatrice points → training data → surrogate model (SS_models/<shape>-walls/model.pt)
                                                    ↓
Runtime:                                        scalers.dat
  atom->x, atom->quaternion, atom->shape_key
                    ↓
            surrogate model (SS_models/<shape>-walls/model.pt)
                    ↓
            overlap, contact point, normal
                    ↓
            physics model (Hertz, Hooke, etc.)
                    ↓
            force
```

## ss-wall Contact (Primitive Walls)

In `fix_wall_gran.cpp`, add surrogate branch:
```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag) {
    // 1. z_unit = (deltan + radius) / L ;  skip if z_unit > z_prefilter
    // 2. d = delta/|delta| ;  raw = model(z_unit, q)  (float32)
    // 3. sd_unit = raw*y_scale + y_mean - h_exact(q, d) ;  deltan = sd_unit * L
    // 4. warn if sd_unit < sd_too_deep
    // 5. Hard-zero contact history and `continue` if deltan > 0
    // 6. Otherwise native physics model with penetration -deltan
}
#endif
```

Key: overlap AND contact point/normal from SAME surrogate evaluation.

## ss-ss Contact (Future)

In `pair_gran.cpp` or new `pair_surrogate.cpp`:
- Surrogate predicts overlap + contact point + normal
- Model selected by shape key pair
- Each (shape_i, shape_j) pair has its own trained model

## Contact History

Replicate superquadric pattern:
```cpp
if(atom->surrogate_flag && deltan > 0.0) {
    if(c_history)
        vectorZeroizeN(c_history[iPart], dnum_);
    continue;   // next particle; `break` would skip the rest of the neighbour list
}
```

## Model Selection

```cpp
// At init: read shape names from input file, populate shape database
// At runtime: use shape key (int) for fast model lookup
int shape_key = atom->shape_key[ip];
SurrogateModel* model = shape_database.get_model(shape_key, object_type);
```

## Tilted / rotating cubes (R04.2b–R04.2e, 2026-11-17)

**Per-shape geometry layer** (`surrogate_interface.h`, namespace `SurrogateShape`): a struct
of shape constants + functions, selected by `h_exact_method` from scalers.dat. Only the cube
(`analytical_rotation_matrix`) exists; an unknown method is an error.

| Item | Cube |
|---|---|
| `support_height(q, d)` | `0.5 (|u_x|+|u_y|+|u_z|)`, `u = Rᵀd` |
| `contact_point(q, d, sd)` (STOPGAP R04.2b) | penetration-weighted mean of vertices below the plane, projected on the plane |
| `bounding_radius` | `r_circ` (scalers.dat) = √3/2 |
| `inertia_factor k_I` | 1/6 (I = k_I m L², isotropic) |
| `volume_factor k_V` | 1 (V = k_V L³) |

**fix_wall_gran (surrogate block):**
1. `radius_eff = r_circ·L` for the neighbour list and the `deltan` contact gate (RF-10).
2. After the model query: `sidata.contact_point = x + L·cp_unit`, `sidata.is_non_spherical = true`,
   unit normal `en = -delta/|delta|`, `deltan = sd_phys`.

**surface_model_surrogate.h** (`surface surrogate`): `checkSurfaceIntersect` sets
`is_non_spherical`; `surfacesIntersect` sets `reff = radi` and calls the generic
`MathExtraLiggghtsNonspherical::surfacesIntersectNonSpherical` (velocity at the contact point,
lever arm `cri = |x_c − x|`). No superquadric code.

**Build:** `ENABLE_SURROGATE` defines `NONSPHERICAL_ACTIVE_FLAG` (generic torque-from-contact-point
code only; verified that nothing superquadric-specific is behind it) and enables
`ENABLE_MODEL_SURFACE_SURROGATE`.

**fix nve/sphere (surrogate branch):** `I = k_I m L²`; quaternion update
`q ← Δq(ω dt) ⊗ q` each full step (same algorithm as `MathExtraLiggghtsNonspherical::integrate_quat`).

**compute erotate/surrogate:** `Σ ½ k_I m L² |ω|²`.
