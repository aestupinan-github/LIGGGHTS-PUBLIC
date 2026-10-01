# Requirements: CTest Testing Infrastructure

## Status

Draft

## Context and Objective

The project currently has no automated testing. The CI workflow runs `ctest` but finds zero tests, so it passes vacuously. The objective is to establish a testing infrastructure that:

1. Provides a CTest-based framework for defining and running tests.
2. Supports both unit tests (isolated logic checks) and integration tests (full simulation runs with output comparison).
3. Enables tests to be added incrementally as new features are developed.
4. Catches regressions in existing functionality.
5. Runs automatically in CI.

## Scope

**Included:**

- CTest-based test infrastructure (no external test framework).
- Unit tests using assert-based checks with return codes.
- Integration tests that run simulations and compare output against reference data.
- Per-test tolerance configuration with a default tolerance.
- Analytical references where possible; golden files for complex cases.
- CI integration so tests run automatically on push/PR.
- Regression tests for critical existing functionality.
- Test-first capability: tests can be written for features before implementation.

**Excluded:**

- External test frameworks (Google Test, Catch2, doctest).
- Figure generation / plotting (deferred).
- Performance benchmarking.
- Parallel/MPI-specific testing.

## Definitions

- **Unit test**: A test that verifies isolated logic (e.g., a mathematical function, a data structure operation) without running a full simulation.
- **Integration test**: A test that runs a full simulation and compares output against reference data.
- **Golden file**: A reference output file generated from a known-good build, used for comparison.
- **Analytical reference**: A mathematically exact expected result derived from theory.
- **Tolerance**: The maximum acceptable difference between a computed value and its reference.
- **Default tolerance**: The tolerance applied when a test does not specify its own.
- **Test-first**: Writing a test before the corresponding implementation exists.

## Functional Requirements

- RF-1: THE SYSTEM SHALL provide a CTest-based infrastructure for defining, building, and running tests.
- RF-2: THE SYSTEM SHALL support unit tests that verify isolated logic using assert-based checks and return codes.
- RF-3: THE SYSTEM SHALL support integration tests that run simulations and compare output against reference data.
- RF-4: WHEN an integration test runs, THE SYSTEM SHALL compare simulation output against reference data using a specified tolerance.
- RF-5: WHEN a test does not specify its own tolerance, THE SYSTEM SHALL apply the default tolerance.
- RF-6: WHEN a test specifies its own tolerance, THE SYSTEM SHALL use that tolerance instead of the default.
- RF-7: THE SYSTEM SHALL support analytical references where mathematically exact results are available.
- RF-8: THE SYSTEM SHALL support golden files for cases where analytical references are not available.
- RF-9: THE SYSTEM SHALL report test pass/fail status with diagnostic output on failure.
- RF-10: THE SYSTEM SHALL run all tests automatically in CI on push and pull request events.
- RF-11: THE SYSTEM SHALL allow tests to be added incrementally as new features are developed.
- RF-12: THE SYSTEM SHALL support test-first development: a test may be written before the corresponding implementation exists.
- RF-13: IF a test fails, THEN THE SYSTEM SHALL provide sufficient diagnostic output to identify the cause.
- RF-14: THE SYSTEM SHALL support regression tests for critical existing functionality.

## Numerical and Physical Requirements

- NR-1: THE SYSTEM SHALL use a default tolerance of 1e-6 for integration tests unless overridden per-test.
- NR-2: WHEN comparing floating-point values, THE SYSTEM SHALL use relative error, absolute error, or a combination thereof, as appropriate for the quantity being tested.
- NR-3: THE SYSTEM SHALL document the tolerance and comparison method for each test.
- NR-4: THE SYSTEM SHALL produce deterministic results for tests that do not involve random number generation.
- NR-5: WHEN a test involves random number generation, THE SYSTEM SHALL fix the random seed to ensure reproducibility.

## Non-Functional Requirements

- NFR-1: THE SYSTEM SHALL build and run tests without requiring external dependencies beyond what the main build already requires.
- NFR-2: THE SYSTEM SHALL produce clear, actionable error messages when a test fails.
- NFR-3: THE SYSTEM SHALL run tests in a reasonable time (unit tests in seconds, integration tests in minutes).
- NFR-4: THE SYSTEM SHALL be portable across Linux platforms.
- NFR-5: THE SYSTEM SHALL not interfere with the main build or require a separate build directory.

## Edge Cases

- EC-1: WHEN a test references a golden file that does not exist, THE SYSTEM SHALL report a clear error.
- EC-2: WHEN a test references an analytical reference that is not available, THE SYSTEM SHALL skip the test with a warning (not a failure).
- EC-3: WHEN a simulation produces NaN or Inf values, THE SYSTEM SHALL fail the test with a descriptive message.
- EC-4: WHEN a test is run without a prior successful build, THE SYSTEM SHALL report a clear error.
- EC-5: WHEN the default tolerance is changed, THE SYSTEM SHALL apply the new default to all tests that do not override it.

## Out of Scope

- External test frameworks (Google Test, Catch2, doctest).
- Figure generation and plotting.
- Performance benchmarking and profiling.
- Parallel/MPI-specific testing.
- GUI or web-based test reporting.
- Test coverage reporting.

## Completion Criteria

- CC-1: CTest infrastructure is configured and `ctest` discovers and runs tests.
- CC-2: At least one unit test passes.
- CC-3: At least one integration test passes.
- CC-4: CI workflow runs tests automatically on push/PR.
- CC-5: A test-first test for a pending feature can be written and fails for the expected reason (feature not implemented).
- CC-6: Regression tests for critical existing functionality pass.
- CC-7: Test failures produce actionable diagnostic output.

## Open Questions

- [NEEDS CLARIFICATION] What specific existing functionality should be covered by regression tests? (To be determined as development progresses.)
- [NEEDS CLARIFICATION] Should the default tolerance be adjusted based on experience with initial tests?
