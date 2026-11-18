# Reference results — surrogate contact test cases

Frozen results used as a regression reference. Do **not** overwrite by hand; `run_tests.sh`
writes new runs to `../output/` (git-ignored). Compare a new run against these files.

- Regenerated: 2026-11-17 (after Spec 04 R04.2b–e: unit normal, corner detection, contact-point
  STOPGAP, quaternion integration). Originally generated 2026-11-17, branch `surrogate-shape-dev`, binary built with
  `-DENABLE_SURROGATE=ON` (LibTorch), model `SS_models/ss-cube-walls` = run_029 (option4),
  scalers y_mean 0.74250850860714912, y_scale 0.13142255689813576.
- Contract: Spec 00 R00.1–R00.5 (revised 2026-11-17); L = particle diameter (Spec 03 R03.3).
- Each folder: `log.liggghts`, `output.log` (stdout+stderr), `post/*.vtk` (ParaView; id, type,
  x, v, f, omega, radius, mass, orientation as `TENSOR`). test_01c also has `post/dump_contact.lammpstrj`.

| Folder | Input | L | dt | VTK every | Particle at end |
|---|---|---|---|---|---|
| test_1 | test_01_cube_1m_falling.in | 1 m | 1e-4 | 100 | present, rebounding |
| test_01b_small_dt | test_01b_small_dt.in | 1 m | 5e-5 | 200 | present, rebounding |
| test_01c_diagnostic | test_01c_diagnostic.in | 1 m | 1e-4 | 10 | present, rebounding |
| test_2 | test_02_cube_1cm_falling.in | 0.01 m | 1e-5 | 100 | present, rebounding |
| test_03_cube_1cm_corner | test_03_cube_1cm_corner.in | 0.01 m | 1e-5 | 100 | present, rebounding |
| test_03b_cube_1cm_corner_tilt25 | test_03b_cube_1cm_corner_tilt25.in | 0.01 m | 1e-5 | 100 | present, spinning after impact |
| test_04_cube_1cm_edge | test_04_cube_1cm_edge.in | 0.01 m | 1e-5 | 100 | present, rebounding |
| test_05a/b/c_cube_1cm_spin_h0xx | test_05[abc]_*.in | 0.01 m | 1e-5 | 50 | present, tumbling |

Key numbers (from the VTK data; see Spec 04 "Validation Results"):
- test_01c: restitution 0.899 (input 0.9), max penetration 0.0444 L, contact ≈ 9.1 ms
- test_02 (separate run with VTK every step): restitution 0.900, max penetration 0.0070 L, contact ≈ 0.160 ms

Tilted / spinning cases (Spec 04 R04.2b–e; dense re-runs with VTK every step, see Spec 04):
- test_03 corner: contact starts when the corner reaches the wall; e = 0.905; |ω| stays 0 (4e-14)
- test_03b corner tilted 25° about x, no initial spin: impact spins the cube about x, ω = 227 rad/s
  (superquadric n = 1000: 225 rad/s)
- test_04 edge: e = 0.898; |ω| stays 0 (6e-14)
- test_05c: impact on a corner off-centre → torque during the 25 contact steps; total energy never
  rises above its initial value in any case (max(E)/E0 = 1.0)

Note: test_01/01b/01c/02 changed slightly vs. the first reference because the wall normal passed
to the contact law is now a unit vector (RF-8c; before it was (L/2 − pen)/(L/2 + pen) long, i.e.
the force was under-estimated by up to 16 %): test_01c restitution 0.899 → 0.900,
max penetration 0.0444 L → 0.0422 L.
