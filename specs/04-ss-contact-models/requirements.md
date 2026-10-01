# Spec 04 — Geometric Contact Models (Surrogate-Based)

## Requirements

### Scope
- **Geometric** contact models only (surrogate predicts overlap, contact point, normal)
- **Physics** contact models (Hertz, Hooke, etc.) are unchanged — they consume surrogate output
- Do NOT confuse geometric and physics contact models

### ss-wall Contact (NOW)
- Surrogate predicts overlap for primitive walls (plane, cylinder, etc.)
- Both overlap magnitude AND contact point/normal derived from surrogate's own consistent evaluation
- Do NOT reuse native superquadric closest-point search for contact point while using surrogate only for overlap

### ss-ss Contact (FUTURE — exploratory)
- Surrogate predicts overlap + contact point + normal for shape-shape pairs
- Each (shape_i, shape_j) pair gets its own trained model
- No prior art in this project — treat as open/exploratory

### Mesh Walls (FUTURE)
- Separate shape type for now
- Could be treated like primitives in the future

### Contact History
- Hard-zeroing when deltan > 0 (replicate superquadric pattern at fix_wall_gran.cpp:1395-1401)
- When contact is broken, zero contact history and break

### Model Selection
- Input file specifies shape name → look up model → assign int key → runtime uses key
- For ss-wall: model selected by shape key alone
- For ss-ss: model selected by both shape keys

### Pair Style
- Explicit `pair_style` selection + cutoff control (LIGGGHTS convention)
- Runtime warning if surrogate particles present but no surrogate pair style active
