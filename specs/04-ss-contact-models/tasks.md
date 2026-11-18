# Spec 04 — Geometric Contact Models: Tasks

- [x] Implement ss-wall contact in fix_wall_gran.cpp (overlap only; contact point/normal deferred) — corrected to run_029 contract and verified at L=1 m and L=0.01 m (2026-11-17)
- [x] Implement contact history hard-zeroing for surrogate (`continue`, not `break`)
- [ ] Add runtime warning if surrogate particles present but no surrogate pair style active
- [ ] (FUTURE) Implement ss-ss contact in pair_gran.cpp
- [ ] (FUTURE) Implement mesh-wall contact
- [x] Prefilter (z_prefilter) and too-deep warning (sd_too_deep) from scalers.dat
- [x] R04.2d: corner/edge detection with r_circ·L (EC-4) — test_03 contact starts at z_c = 0.866 L
- [x] R04.2b STOPGAP: contact point from h_exact geometry in LIGGGHTS (RF-8, RF-8a, RF-8c) — verified 2026-11-17
- [ ] **MANDATORY, OPEN** R04.2b RF-8b: replace stopgap by surrogate contact-point output; remove all `STOPGAP (Spec 04 R04.2b)` sites
- [ ] Send the contact-point request to the surrogate (training) team
- [x] R04.2c: surface model `surface surrogate` (new file `surface_model_surrogate.h`, explicit selection)
- [x] R04.2e: quaternion integration + cube inertia in fix nve/sphere; `set quat`/`quat_direct`/`quat/random`; `compute erotate/surrogate`
- [x] Test cases (L = 1 cm): test_03 corner, test_04 edge, test_05a/b/c spinning at 3 heights; regression test_01/02 — reference/ updated
