# Design: Multi-Particle Surrogate Test

## Status

Implemented 2026-11-18

## Files

| File | Content |
|---|---|
| `test_cases/surrogate_multi/in.pour_125_cubes` | input (header documents the placeholder pair contact) |
| `test_cases/surrogate_multi/reference/np{1,2,4}/output.log` | frozen logs (np1 also VTK `post/`, every 2000 steps) |
| `tests/CMakeLists.txt` | `surrogate_regression_pour_125_cubes` (np1 vs reference, rtol 1e-6) |

## Setup

Box 0.12 × 0.12 × 0.20 m, 6 `wall/gran ... surface surrogate` planes; 5×5×5 lattice, spacing
2.0 L, random shift ≤ 0.05 L, `set quat/random`; ρ = 1000, E = 1e7 Pa, ν = 0.25, e = 0.6,
μ = 0.4, dt = 1e-5 s, 30 000 steps; `pair_style gran ... surface default` (placeholder).

## Bugs found and fixed while building this test

1. `AtomVecSurrogate::grow(0)` did not increase `nmax` → arrays never grew; the 2nd created atom
   wrote out of bounds (segfault with > 1 particle). Fixed; also removed an uninitialised `DELTA`
   member that shadowed the macro.
2. `PairGranBase::compute_force` set the sphere contact normal `sidata.en` only for
   `atom_style sphere`; for surrogate particles with `surface default` it stayed uninitialised →
   wrong force direction, energy gain (two cubes stuck together instead of bouncing). Fixed
   (dual-guarded `|| atom->surrogate_flag`). **TEMPORARY** — belongs to the sphere placeholder and
   must be removed with Spec 04 R04.7 (see Spec 04 tasks, code marker `PLACEHOLDER (Spec 04 R04.7)`).
   Fix 1 (`grow()`) is permanent.
3. First input versions placed cubes inside walls / overlapping each other (lattice offsets),
   which injects energy at step 0. Fixed by the NR-1 geometry.
