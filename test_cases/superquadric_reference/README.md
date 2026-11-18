# Superquadric reference cases for the surrogate cube

Independent reference (native LIGGGHTS superquadric geometry) for the surrogate cube cases
in `../surrogate_contact/`. Same setup: L = 1 cm, ρ = 1000 kg/m³, Hertz + tangential history,
E = 1e9 Pa, ν = 0.25, e = 0.9, μ = 0.2, dt = 1e-5 s, same drop height, orientation and spin.

| Input | Matches | Case |
|---|---|---|
| `sq_04_cube_1cm_edge.in` | `test_04_cube_1cm_edge.in` | edge down (45° about x), no spin |
| `sq_05c_cube_1cm_spin_h010.in` | `test_05c_cube_1cm_spin_h010.in` | generic tilt, ω₀ = (50, 30, 0) rad/s, h = 0.10 m |

The cube is a superquadric with half-axes L/2 and **blockiness 50**: corners are rounded by
0.019 L (0.09 L at blockiness 10), mass 0.998 g instead of 1.000 g.

## Build (separate binary; does not touch the surrogate build)

The generated `src/style_*.h` files are shared by every CMake build of the same source tree,
so build the superquadric binary from a **copy** of `src/`:

```bash
git archive HEAD src | tar -x -C /tmp/sq_src
cmake -B /tmp/sq_src/build /tmp/sq_src/src -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_SQ=ON -DENABLE_MODEL_SURFACE_SUPERQUADRIC=ON -DENABLE_SURROGATE=OFF
cmake --build /tmp/sq_src/build -j
# run from a scratch folder, e.g. output/<case>/:
/tmp/sq_src/build/liggghts -in ../../sq_04_cube_1cm_edge.in
```

Input notes: `set ... shape ... blockiness ... density` must come first (it defines volume,
mass, inertia); `quat_direct` before `omegax/y/z` (spin is converted to angular momentum with
the orientation and inertia). Integrator `fix nve/superquadric integration_scheme 1`.

## Results (2026-11-18) — see Spec 04 "Validation against superquadric reference"

Edge case (first impact): surrogate vz ratio 0.898, SQ(n=50) 0.901; contact 17 vs 16 steps;
no spin in either. Spinning case (first impact): rebound vz 0.006 vs 0.079 m/s, |ω| 253 vs 248 rad/s;
extrapolating SQ n = 10/20/50 to a sharp corner gives vz 0.004 m/s, |ω| 253.0 rad/s.

`reference/` holds frozen logs and VTK of both cases (blockiness 50).
