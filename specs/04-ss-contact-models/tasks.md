# Spec 04 — Geometric Contact Models: Tasks

- [x] Implement ss-wall contact in fix_wall_gran.cpp (overlap only; contact point/normal deferred) — corrected to run_029 contract and verified at L=1 m and L=0.01 m (2026-11-17)
- [x] Implement contact history hard-zeroing for surrogate (`continue`, not `break`)
- [ ] Add runtime warning if surrogate particles present but no surrogate pair style active
- [ ] (FUTURE) Implement ss-ss contact in pair_gran.cpp
- [ ] (FUTURE) Implement mesh-wall contact
- [x] Prefilter (z_prefilter) and too-deep warning (sd_too_deep) from scalers.dat
- [ ] Resolve EC-4: late detection of tilted-cube corner contacts (sphere-based neighbour list)
- [ ] **MANDATORY** R04.2b: contact point for torque (normal and friction). Blocked on: surrogate team model output (request in `DOCUMENTATION.md/REQUEST_SURROGATE_TEAM_CONTACT_POINT_20261117.md`) or user approval of the option4 exact support point
- [ ] Send the contact-point request to the surrogate (training) team
