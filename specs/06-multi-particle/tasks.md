# Tasks: Multi-Particle Surrogate Test

## Status

Completed (2026-11-18)

- [x] **T1. Input with 125 cubes, 6 walls, placeholder pair contact documented.** RF-1, RF-4
- [x] **T2. Fix bugs exposed by the test** (grow(), pair normal, initial overlaps). RF-1, RF-2
- [x] **T3. Serial run: 125 particles kept, E_tot never above E₀, no sd_too_deep warning.** RF-1, RF-2
  - Result: E_tot 0.1227 → 0.0244 J (monotone at thermo resolution), 1.76 s for 30 000 steps.
- [x] **T4. MPI 2 and 4 ranks.** RF-3
  - np2 identical to np1 at every thermo row; np4 identical up to step 10 230, then round-off
    (~1e-11) growth from a different reduction order, max |Δz_c| 1.9e-6 m, |ΔE| 1.1e-5 J at
    30 000 steps (chaotic many-body dynamics). Same seen with pair contacts switched off.
- [x] **T5. CTest regression `surrogate_regression_pour_125_cubes`.** RF-5
- [ ] **T6. (future)** Replace the placeholder pair contact by a surrogate cube–cube model (Spec 04 R04.7).
