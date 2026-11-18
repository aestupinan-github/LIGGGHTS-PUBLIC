# Restart files not supported for `atom_style surrogate`

Status: needs-triage
Type: bug
Spec: 03-surro-shape-type R03.10 (known limitation), R03.9
Reported: 2026-11-17 (found during the multi-shape audit; no current input uses restart)

## Summary

`write_restart` / `read_restart` cannot be used with `atom_style surrogate`. Nothing in the
current test cases uses restart files, so nothing is broken today, but a user who adds
`restart N file` / `read_restart file` will get an error or a corrupted state.

## Evidence (source, 2026-11-17)

1. **Restore fails at once.** `read_restart.cpp:686` calls `atom->create_avec(style,nwords,words)`
   with `nwords = 0` for non-hybrid styles. `AtomVecSurrogate::settings()`
   (`atom_vec_surrogate.cpp:86-95`) requires exactly one argument (the shape name) and aborts:
   "Illegal atom_style surrogate command".
2. **Shape database not saved.** `AtomVecSurrogate` does not override
   `write_restart_settings` / `read_restart_settings` (defaults in `atom_vec.h` are empty), so the
   shape names (`atom->surrogate_shape_names`) are not in the file and a stored `shape_key`
   cannot be mapped back to a model.
3. **Size mismatch.** `size_restart()` returns `14 + 4 = 18`; `pack_restart()` writes 17 values
   (`m` starts at 1, 14 base values, + `characteristic_length` + `shape_key`).
4. **Missing per-atom state.** `pack_restart()` / `unpack_restart()` omit `radius`, `rmass`,
   `density`, `omega`, `angmom` — a restarted particle would lose its mass, size and spin.

## Expected

A simulation with surrogate particles can be written to a restart file and continued with
`read_restart` with identical state (positions, velocities, orientation, spin, mass, radius,
characteristic length, shape key, shape database).

## Suggested fix

- Implement `write_restart_settings` / `read_restart_settings` to store and restore the shape
  database (names in key order), and let `settings()` accept zero arguments when called from
  `read_restart` (shapes then come from `read_restart_settings`).
- Pack/unpack radius, rmass, density, omega, angmom; make `size_restart()` equal the packed count.
- Test: run test_05 to step N, write restart, continue from it, compare VTK output at step 2N
  with an uninterrupted run (bitwise or 1e-12).

## Comments
