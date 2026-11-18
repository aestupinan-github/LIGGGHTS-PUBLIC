# Requirements: Multi-Particle Surrogate Test

## Status

Approved (scope agreed with user 2026-11-18) — implemented

## Context and Objective

All surrogate tests so far used one particle. This spec adds the first test with many surrogate
particles to check the software at a realistic scale: many simultaneous wall contacts (batched
model calls, Spec 04 R04.2f), several walls, particle–particle neighbour handling, and runs on
several MPI processes (ghost communication fixed in Spec 03 R03.9 but never exercised).

## Scope

Included: 125 cubes (L = 1 cm) with random orientations falling into a closed box of 6 primitive
walls; serial and MPI (2, 4 ranks); regression test of the serial run.
Excluded: physical packing results — cube–cube contact has no surrogate model yet (Spec 04 R04.7).

## Definitions

- **Placeholder pair contact**: cube–cube collisions computed by `pair_style gran ... surface
  default`, i.e. as spheres of the bounding radius 0.866 L.

## Functional Requirements

- RF-1: THE SYSTEM SHALL run 125 surrogate cubes for 30 000 steps without errors or lost particles.
- RF-2: THE SYSTEM SHALL NOT create energy: E_tot = KE + E_rot + E_pot SHALL never exceed its
  initial value (thermo every 1000 steps) when no particle starts in contact.
- RF-3: THE SYSTEM SHALL give the same results on 1, 2 and 4 MPI ranks up to round-off growth.
- RF-4: THE SYSTEM SHALL state in the input that cube–cube contact is a sphere placeholder.
- RF-5: The serial run SHALL be part of the CTest regression suite.

## Numerical and Physical Requirements

- NR-1: Initial configuration with no overlaps: bounding spheres separated (lattice 2.0 L) and
  ≥ 1 L clearance to the walls beyond the bounding radius.
- NR-2: Random seeds fixed (LIGGGHTS requires primes > 10 000: 15013, 15017).

## Completion Criteria

- CC-1: RF-1..RF-5 verified and recorded (Spec 06 tasks, MEMORY.md).
