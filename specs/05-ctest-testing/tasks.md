# Tasks: CTest Testing Infrastructure

## Status

Completed (2026-11-18) — 10 tests in surrogate builds, 1 in default builds; T8 (analytical sphere test) still open

- [x] **T1. Add `enable_testing()` to CMake configuration.** RF-1, RF-10
  - Done when: `enable_testing()` is called in the top-level `src/CMakeLists.txt` and `ctest` discovers zero tests without error.

- [x] **T2. Create test directory structure.** RF-1, RF-11
  - Done when: `tests/` directory exists with `unit/` and `integration/` subdirectories, and a `CMakeLists.txt` that registers tests with CTest.

- [x] **T3. Implement unit test helper macro.** RF-2, RF-12
  - Done when: A CMake macro or function exists that registers a CTest test from a C++ source file using `assert()` and return codes.

- [x] **T4. Implement integration test helper.** RF-3, RF-4, RF-7, RF-8
  - Done when: A CMake function exists that runs `liggghts -in <script>` and compares output against a reference (analytical or golden file) with a specified tolerance.

- [x] **T5. Add per-test tolerance mechanism.** RF-4, RF-5, RF-6
  - Done when: Tests can specify their own tolerance via a parameter, with a default of 1e-6 when not specified.

- [x] **T6. Add diagnostic output on test failure.** RF-9, RF-13
  - Done when: Failed tests produce output showing expected vs actual values and the tolerance used.

- [x] **T7. Update CI workflow to run tests.** RF-10
  - Done when: `.github/workflows/cmake-single-platform.yml` runs `ctest --output-on-failure` after build and fails if any test fails.

- [ ] **T8. (PENDING) Add first integration test: analytical reference.** RF-3, RF-4, RF-7
  - Done when: A test exists that runs a simple simulation and compares output against an analytical solution with tolerance 1e-6.
  - Note: Implement later using one of the tutorials or a simple case.

- [x] **T9. Regression test: sphere tutorial (`cohesion`) runs to completion.** RF-14
  - Done when: A test exists that runs a bouncing sphere simulation and verifies it completes without error and produces expected output files.
  - Note: Implement later using existing bouncing sphere case.

- [x] **T10. Surrogate unit tests** (geometry, canonicalization, denormalization, rescaling). RF-15, RF-16, RF-17
  - Done when: `ctest -R surrogate_geometry` passes; a deliberate wrong value makes it fail with expected/actual output.
- [x] **T11. Surrogate model unit test** (run_029 vs Python reference). RF-18
  - Done when: `ctest -R surrogate_model` passes within 2e-4.
- [x] **T12. Surrogate regression tests** (test_02, 03, 03b, 04, 05c vs reference metrics). RF-19
  - Done when: all pass with the current binary; each fails if a key metric is perturbed.
- [x] **T13. Error-handling tests** (unknown model_type / missing models dir). RF-20
- [x] **T14. Non-surrogate build + sphere regression** (cohesion tutorial). RF-21, RF-22
- [x] **T15. CI uses `ctest --output-on-failure`.** RF-10

## Result (2026-11-18)

`ctest --test-dir build --output-on-failure` (surrogate build): 10/10 pass in ≈ 7 s (with `-j 4`):
`sphere_cohesion_tutorial_runs`, `surrogate_geometry` (33 checks), `surrogate_model` (11 checks),
`surrogate_regression_test_02/03/03b/04/05c` (201–401 thermo rows × 6 columns, rtol 1e-6),
`surrogate_error_unknown_shape`, `surrogate_error_surface_needs_surrogate`.
Default build (ENABLE_SURROGATE=OFF): 1/1 (sphere tutorial). Negative checks done: a wrong
expected value fails `surrogate_geometry` with expected/actual output; a changed restitution
(0.9 → 0.89) fails the regression check.

**Updating references** (after an intended physics change): re-run the case and copy its
`output.log` into `test_cases/surrogate_contact/reference/<case>/`, and record why in MEMORY.md.

**Known LIGGGHTS behaviour:** `Error::one()` calls `MPI_Abort` without flushing stdout; error
tests therefore run through `stdbuf -o0 -e0`.
