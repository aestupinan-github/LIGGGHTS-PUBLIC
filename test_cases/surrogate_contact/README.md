# Surrogate Contact Test Cases

Test suite for Spec 04 (Contact Models) validation. These tests verify that the surrogate contact model works correctly for different characteristic lengths and wall orientations.

## Test 01: Cube (L=1m) Falling on Plane

**File:** `test_01_cube_1m_falling.in`

**Purpose:** Test surrogate contact with trained characteristic length

**Setup:**
- Particle: Single cube with L_actual = 1.0 m (matches training scale)
- Initial height: z = 10 m
- Wall: Primitive plane at z = 0
- Duration: 2 seconds (20,000 timesteps @ dt=0.0001 s)

**Expected Behavior:**
1. Cube falls freely for ~1.41 seconds (t = sqrt(2*h/g) = sqrt(2*10/9.81) ≈ 1.43s)
2. Velocity at impact: v = sqrt(2*g*h) = sqrt(2*9.81*10) ≈ 14 m/s
3. Cube contacts wall via surrogate model
4. Contact force > 0 after impact
5. Cube bounces back (Hertz elastic contact)

**Model Used:**
- run_029 (ss-cube-walls, 84.7% accuracy on training set)
- Input: [z_unit, qw, qx, qy, qz] where z_unit ∈ [-0.5, 2.0] during training
- Output: log10(overlap)

**Physics Model:**
- Hertz normal contact (elastic)
- Tangential history (friction)
- No cohesion, no rolling friction (simple case)

---

## Test 02: Cube (L=1cm) Falling on Plane

**File:** `test_02_cube_1cm_falling.in`

**Purpose:** Test L-rescaling capability (model trained at L=1m, applied at L=0.01m)

**Setup:**
- Particle: Single cube with L_actual = 0.01 m (100x smaller than training scale)
- Initial height: z = 0.1 m (scaled by same factor)
- Wall: Primitive plane at z = 0
- Duration: 0.2 seconds (20,000 timesteps @ dt=0.00001 s, 100x smaller timestep)

**Expected Behavior:**
1. Cube falls freely for ~0.141 seconds
2. Velocity at impact: v ≈ 1.4 m/s (100x smaller than test_01)
3. Cube contacts wall via rescaled surrogate model
4. Contact force ~1e-6 times test_01 (due to 100^3 volume ratio)
5. Cube bounces back with much smaller accelerations

**Model Used:**
- Same run_029 model as test_01, but applied with L_actual = 0.01 m
- Denormalization: overlap_phys = overlap_unit * L_actual
- Input scaling: z_unit = z_phys / L_actual (computed from wall distance)

**Physics Model:**
- Same Hertz + tangential as test_01
- Scaled down for smaller particles

---

## Key Design Validation

These tests verify Spec 04 Phase 3C design decisions:

### **Generalized Normal Distance (Q4)**
- Both tests use delta from primitiveWall_->resolveContact() with any orientation
- Wall is in xy-plane (z-normal); tests verify contact calculation works

### **Contact Cutoff Strategy (Q7)**
- Surrogate queried only if deltan <= cutoff (near-field)
- Far-field: particles too far above wall → no force

### **L-Rescaling (Q6)**
- Test 02 demonstrates overlap_phys = overlap_unit * L_actual
- Same model applied at 100x smaller scale; contact force scales accordingly

### **Skip Threshold (Q10)**
- If s_unit > 3 * L_actual → skip contact
- Both tests designed to stay within trained range

---

## Running the Tests

**Prerequisite:** Build LIGGGHTS with surrogate support
```bash
cmake -B build ./src/ -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_SURROGATE=ON \
  -DLIBTORCH_DIR=/path/to/libtorch \
  -DENABLE_MPI=ON
cmake --build build -j
```

**Run Test 01:**
```bash
cd /path/to/LIGGGHTS-PUBLIC
./build/liggghts -in test_cases/surrogate_contact/test_01_cube_1m_falling.in
```

**Run Test 02:**
```bash
./build/liggghts -in test_cases/surrogate_contact/test_02_cube_1cm_falling.in
```

---

## Expected Output

### Test 01
```
Timestep   Atoms   KE           PE           Total E
0          1       0.0          98100.0      98100.0
...
14300      1       97480.0      620.0        98100.0   (free fall, ~1.43s)
14400      1       45000.0      53100.0      98100.0   (impact, bouncing)
...
20000      1       500.0        97600.0      98100.0   (settling)
```

### Test 02
```
Timestep   Atoms   KE           PE           Total E
0          1       0.0          0.00981      0.00981
...
1410       1       0.00932      0.000490     0.00981   (free fall, ~0.141s)
1420       1       0.004500     0.005310     0.00981   (impact, bouncing)
...
20000      1       0.0000050    0.009800     0.00981   (settling)
```

**Key Observations:**
- Energy should be roughly conserved (elastic collisions)
- Peak kinetic energy near impact
- Damping due to Hertz contact (energy loss)
- Test 02 values should be ~1e-6 times test_01 values

---

## Debugging Notes

### If Test Fails:

1. **"Surrogate particles require model loaded; check init() phase"**
   - Model not found or load failed
   - Check: `SS_models/ss-cube-walls/model.pt` and `scalers.dat` exist
   - Check: Model path resolver can find files

2. **"Invalid characteristic_length"**
   - Characteristic length not set or ≤ 0
   - Verify `set atom N diameter L_actual` command in input file

3. **Contact force = 0 after impact**
   - Surrogate model may not be queried
   - Check: deltan <= cutoff threshold
   - Check: s_unit within [-0.5, 2.0] (training range)
   - Check: Quaternion is valid (canonicalized)

4. **Simulation crashes**
   - Likely missing header file or torch API mismatch
   - Check: Build completed without errors
   - Check: `#ifdef SURROGATE_ACTIVE_FLAG` guards are in place

---

## Future Test Cases (Phase 3D+)

- [ ] Multiple cube particles (ss-ss contact)
- [ ] Mesh wall contact (instead of primitive)
- [ ] Different shapes (ss-pyramid, ss-cylinder)
- [ ] Non-orthogonal wall orientations
- [ ] Quaternion randomization (non-aligned cubes)
- [ ] Elongated shapes (test skip threshold Q10)
- [ ] Stress/strain analysis (wall forces)

---

## Spec References

- **Spec 04:** Contact Models
  - R04.1: Surrogate is sole geometric source
  - R04.2: 7-step pipeline (input → denormalization → output)
  - R04.6: Hard-zero contact history on break
  
- **Spec 00:** Interface Contract
  - R00.1: Input normalization (z_unit = s_unit)
  - R00.2: Output denormalization formula
  
- **Spec 03:** Shape Type
  - R03.1: Shape registration
  - R03.2: Input syntax `atom_style surrogate ss-cube`

