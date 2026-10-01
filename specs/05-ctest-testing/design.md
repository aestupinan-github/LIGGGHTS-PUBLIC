# Design: CTest Testing Infrastructure

## Status

Draft

## Requirements Traceability

| Design Element | Requirements Covered |
|---|---|
| CMake `enable_testing()` | RF-1, RF-10 |
| Test directory structure | RF-1, RF-11 |
| Unit test helper macro | RF-2, RF-12 |
| Integration test helper function | RF-3, RF-4, RF-7, RF-8 |
| Per-test tolerance mechanism | RF-4, RF-5, RF-6 |
| Diagnostic output on failure | RF-9, RF-13 |
| CI workflow update | RF-10 |
| Initial integration test (pending) | RF-3, RF-4, RF-7 |
| Regression test (pending) | RF-14 |

## Existing Code Context

- **Build system:** CMake (`src/CMakeLists.txt`), with targets `liggghts_obj`, `liggghts_static`, `liggghts_shared`, `liggghts_bin`.
- **CI:** GitHub Actions workflow (`.github/workflows/cmake-single-platform.yml`) runs `ctest` but finds zero tests.
- **No existing tests:** No `tests/` directory, no `add_test()` calls, no test framework.
- **Binary location:** `build/liggghts` (from `liggghts_bin` target).

## Files and Responsibilities

| File | Responsibility | Expected Changes |
|---|---|---|
| `src/CMakeLists.txt` | Top-level CMake configuration | Add `enable_testing()` and `add_subdirectory(tests)` |
| `tests/CMakeLists.txt` | Test registration | Create new file that registers unit and integration tests with CTest |
| `tests/unit/CMakeLists.txt` | Unit test registration | Create new file for unit test targets |
| `tests/integration/CMakeLists.txt` | Integration test registration | Create new file for integration test targets |
| `tests/unit/test_quaternion.cpp` | Unit test for quaternion canonicalization | Create new file (pending) |
| `tests/integration/test_analytical.cpp` | Integration test with analytical reference | Create new file (pending) |
| `tests/integration/test_bouncing_sphere.cpp` | Regression test for bouncing sphere | Create new file (pending) |
| `.github/workflows/cmake-single-platform.yml` | CI workflow | Update to run `ctest --output-on-failure` |

## Interfaces

### CMake Functions/Macros

```cmake
# Register a unit test from a C++ source file
# Usage: add_unit_test(<name> <source_file>)
add_unit_test(<name> <source_file>)

# Register an integration test
# Usage: add_integration_test(<name> <input_script> <reference_file> [tolerance])
add_integration_test(<name> <input_script> <reference_file> [tolerance])
```

### Unit Test Source File Interface

```cpp
// Each unit test is a standalone C++ program
// Returns 0 on success, non-zero on failure
// Uses assert() for checks
int main() {
    // Test logic here
    assert(condition);
    return 0;
}
```

### Integration Test Reference File Format

- **Analytical reference:** A text file with expected values, one per line.
- **Golden file:** A copy of a known-good output file from a previous run.

## Data and Units

- **Tolerance:** Dimensionless floating-point value (default: 1e-6).
- **Comparison method:** Relative error for values >> 1, absolute error for values near zero.
- **Test output:** Text-based, written to stdout/stderr.

## Algorithm

### Unit Test Execution

```
1. Build the test executable from source file
2. Run the executable
3. If exit code == 0: PASS
4. If exit code != 0: FAIL, print stderr
```

### Integration Test Execution

```
1. Run liggghts -in <input_script> -log <log_file>
2. If simulation crashes (non-zero exit): FAIL
3. Parse output file
4. For each value in output:
   a. Get corresponding reference value
   b. Compute error (relative or absolute)
   c. If error > tolerance: FAIL, print expected vs actual
5. If all values within tolerance: PASS
```

### Tolerance Comparison

```
function compare_values(actual, expected, tolerance):
    if |expected| > 1.0:
        error = |actual - expected| / |expected|  // relative
    else:
        error = |actual - expected|              // absolute
    return error <= tolerance
```

## Numerical Method

- **Relative error:** `|actual - expected| / |expected|` for values with magnitude > 1.
- **Absolute error:** `|actual - expected|` for values with magnitude <= 1.
- **Default tolerance:** 1e-6 (can be overridden per-test).

## Design Decisions

### Decision 1: CTest without external framework

**Selected approach:** Use CTest's built-in `add_test()` with standalone C++ executables.

**Justification:** No external dependencies, minimal build complexity, follows CMake conventions.

**Rejected alternatives:**
- Google Test: Heavy dependency, overkill for simple assert-based tests.
- Catch2: Header-only but adds complexity.
- doctest: Header-only but adds complexity.

### Decision 2: Per-test tolerance with default

**Selected approach:** Each test can specify its own tolerance; if not specified, use default (1e-6).

**Justification:** Different quantities have different numerical sensitivities. A single global tolerance is too rigid.

**Rejected alternatives:**
- Single global tolerance: Too strict for some quantities, too loose for others.
- No tolerance (exact match): Too fragile for floating-point comparisons.

### Decision 3: Analytical references where possible

**Selected approach:** Use analytical solutions as reference when available; use golden files only when necessary.

**Justification:** Analytical references are always correct and don't become stale. Golden files can become stale if the code changes legitimately.

**Rejected alternatives:**
- Only golden files: Risk of stale references.
- Only analytical: Not always available for complex simulations.

### Decision 4: Test-first development support

**Selected approach:** Tests can be written before the corresponding implementation exists. They will fail until the implementation is complete.

**Justification:** Supports test-driven development and ensures tests are written alongside features.

**Rejected alternatives:**
- Tests only after implementation: Risk of tests being skipped or written to match implementation rather than requirements.

## Error Handling and Diagnostics

- **Missing golden file:** Report clear error message with file path.
- **Simulation crash:** Report exit code and last few lines of log file.
- **Value mismatch:** Report expected value, actual value, tolerance, and error magnitude.
- **Build failure:** Report compiler errors.

## Testing and Validation Strategy

### Unit Tests

- **Quaternion canonicalization:** Verify that quaternions with qw < 0 are flipped to qw >= 0.
- **Scaler parsing:** Verify that scaler files are parsed correctly.

### Integration Tests

- **Analytical reference:** Run a simple simulation with known analytical solution (e.g., free fall, single particle bounce).
- **Golden file:** Run a complex simulation and compare against known-good output.

### Regression Tests

- **Basic sphere simulation:** Run a basic sphere simulation and verify it completes without error.

### Validation Commands

```bash
# Build
cmake -B build ./src/
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
ctest --test-dir build -R <test_name> --output-on-failure
```

## Risks and Limitations

- **Floating-point reproducibility:** Results may vary across compilers/platforms. Mitigation: Use appropriate tolerances.
- **Golden file maintenance:** Golden files may need updates when code changes legitimately. Mitigation: Document when and how to update.
- **Test execution time:** Integration tests may be slow. Mitigation: Keep test cases small and focused.
- **CI execution time:** Running all tests in CI may slow down development. Mitigation: Run only affected tests on PRs, full suite on main.

## Assumptions

- The `liggghts` binary is built and available at `build/liggghts` before running integration tests.
- Test input scripts are self-contained and don't require external data files (except golden files).
- The CMake build system is the primary build system (not the legacy make build).
