# Spec 04 — Geometric Contact Models: Design

## Architecture

```
Training time:
  STL / generatrice points → training data → surrogate model (overlap_model.pt)
                                                    ↓
Runtime:                                        scalers.dat
  atom->x, atom->quaternion, atom->shape_key
                    ↓
            surrogate model (overlap_model.pt)
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
    // 1. Compute input features: [z_unit, qw, qx, qy, qz]
    // 2. Call surrogate model
    // 3. Get overlap, contact point, normal from surrogate
    // 4. Use surrogate output for force calculation
    // 5. Hard-zero contact history if deltan > 0
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
    break;
}
```

## Model Selection

```cpp
// At init: read shape names from input file, populate shape database
// At runtime: use shape key (int) for fast model lookup
int shape_key = atom->shape_key[ip];
SurrogateModel* model = shape_database.get_model(shape_key, object_type);
```
