# Spec 03 — Surrogate Shape Type: Tasks

- [ ] Add `atom->surrogate_flag` to atom.h
- [ ] Initialize `surrogate_flag = 0` in atom.cpp
- [ ] Create `atom_vec_surrogate.h` with AtomStyle registration
- [ ] Create `atom_vec_surrogate.cpp` with constructor, data allocation
- [ ] Add `ENABLE_SURROGATE` CMake option
- [ ] Implement global shape database (parallel vectors)
- [ ] Implement dual-guard pattern in fix_wall_gran.cpp
- [x] Set characteristic_length from `set ... diameter` (L = D) in set.cpp (R03.3, RF-8) — verified with L=0.01 m test_02 (2026-11-17)
- [x] Fix ghost communication: pack radius, rmass, characteristic_length, shape_key; size_border = packed count (15) (R03.9)
- [x] Generic SS_models probe in path resolver (R03.9)
- [ ] **BUG (open):** restart not supported — `.scratch/surrogate-restart/issues/01-restart-not-supported.md` (R03.10)
- [ ] (Phase 3D) multi-shape: per-particle shape_key input, model registry by shape_key, pair-model paths (R03.6a, R03.9)
- [x] Per-shape inertia factor stored at shape registration (`atom->surrogate_shape_inertia_factor`, from h_exact_method)
