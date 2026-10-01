# Tasks: CTest Testing Infrastructure

## Status

Draft

- [ ] **T1. Add `enable_testing()` to CMake configuration.** RF-1, RF-10
  - Done when: `enable_testing()` is called in the top-level `src/CMakeLists.txt` and `ctest` discovers zero tests without error.

- [ ] **T2. Create test directory structure.** RF-1, RF-11
  - Done when: `tests/` directory exists with `unit/` and `integration/` subdirectories, and a `CMakeLists.txt` that registers tests with CTest.

- [ ] **T3. Implement unit test helper macro.** RF-2, RF-12
  - Done when: A CMake macro or function exists that registers a CTest test from a C++ source file using `assert()` and return codes.

- [ ] **T4. Implement integration test helper.** RF-3, RF-4, RF-7, RF-8
  - Done when: A CMake function exists that runs `liggghts -in <script>` and compares output against a reference (analytical or golden file) with a specified tolerance.

- [ ] **T5. Add per-test tolerance mechanism.** RF-4, RF-5, RF-6
  - Done when: Tests can specify their own tolerance via a parameter, with a default of 1e-6 when not specified.

- [ ] **T6. Add diagnostic output on test failure.** RF-9, RF-13
  - Done when: Failed tests produce output showing expected vs actual values and the tolerance used.

- [ ] **T7. Update CI workflow to run tests.** RF-10
  - Done when: `.github/workflows/cmake-single-platform.yml` runs `ctest --output-on-failure` after build and fails if any test fails.

- [ ] **T8. (PENDING) Add first integration test: analytical reference.** RF-3, RF-4, RF-7
  - Done when: A test exists that runs a simple simulation and compares output against an analytical solution with tolerance 1e-6.
  - Note: Implement later using one of the tutorials or a simple case.

- [ ] **T9. (PENDING) Add regression test: bouncing sphere simulation.** RF-14
  - Done when: A test exists that runs a bouncing sphere simulation and verifies it completes without error and produces expected output files.
  - Note: Implement later using existing bouncing sphere case.
