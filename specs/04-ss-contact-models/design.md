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
