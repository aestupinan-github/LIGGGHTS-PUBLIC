# Reference results — surrogate contact test cases

Frozen results used as a regression reference. Do **not** overwrite by hand; `run_tests.sh`
writes new runs to `../output/` (git-ignored). Compare a new run against these files.

- Generated: 2026-11-17, branch `surrogate-shape-dev`, binary built with
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

Key numbers (from the VTK data; see Spec 04 "Validation Results"):
- test_01c: restitution 0.899 (input 0.9), max penetration 0.0444 L, contact ≈ 9.1 ms
- test_02 (separate run with VTK every step): restitution 0.900, max penetration 0.0070 L, contact ≈ 0.160 ms
