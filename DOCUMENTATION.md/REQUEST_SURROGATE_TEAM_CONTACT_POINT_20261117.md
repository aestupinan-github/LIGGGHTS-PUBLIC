# Request to the surrogate (training) team: contact point output

**From:** LIGGGHTS integration (branch `surrogate-shape-dev`) · **Date:** 2026-11-17
**Status:** drafted, not yet sent · **LIGGGHTS reference:** Spec 04 R04.2b
**Priority:** required — LIGGGHTS currently runs a temporary cube-only stopgap (see below)

## Summary

The run_029 cube–wall model (option4) is integrated in LIGGGHTS and verified for the overlap
(signed distance): C++ reproduces the Python reference to 1e-6, and face-down drops at
L = 1 m and L = 0.01 m behave as expected. To simulate **tilted and rotating** cubes correctly we
also need the **contact point** (where the wall force acts). This was listed as a deferred
feature on your side; it is now required on ours.

## What we need, and what we do not need

- **Contact normal: not needed** for cube–wall. On a plane the force is always along the wall
  normal (face, edge and corner contacts alike); LIGGGHTS already knows it. (It will be needed
  for future cube–cube models, where it can follow a particle face.)
- **Contact point: needed.** The force must act at the touching corner, edge or face, not at the
  cube centre, so that LIGGGHTS can apply the torque `τ = r × F` (normal force and friction).

## Why it matters (numbers)

Lever arm from the cube centre to the contact point, in units of L, for a cube resting on a plane:

| Orientation | In-plane lever arm |
|---|---|
| flat face, edge at exactly 45°, corner with body diagonal vertical | 0 (symmetric) |
| edge, 30° about x | 0.183 |
| corner, generic tilt q = (0.9, 0.3, 0.2, 0.1) | 0.259 |

Without the contact point a cube landing on a corner cannot tip over and its impact cannot change
its spin. Our project rule (as in yours) is that overlap and contact point come from the same
geometric evaluation.

## What LIGGGHTS does meanwhile (temporary)

To proceed with tilted and rotating cube tests, LIGGGHTS computes the contact point itself from
the exact cube geometry of option4 (`h_exact`): the penetration-weighted mean of the cube vertices
below the wall plane, projected onto the plane. This is exact for a cube but:
- it is a deviation from the "single geometric evaluation" rule, accepted only temporarily;
- it is cube-specific and cannot be used for future shapes without an exact geometry formula;
- every code site is marked `STOPGAP (Spec 04 R04.2b)` and will be removed when your model
  provides the point.

Your output should therefore replace this stopgap one-to-one (same definition, see below), so
results do not change discontinuously when we switch.

## Requested output format (proposal, open for discussion)

Per query, in canonical unit-cube space (L = 1), same inputs as today (`position_z`, `quat`):

```
sd                  signed distance (as today)
cp_x, cp_y, cp_z    contact point relative to the cube centre, world frame, units of L
```

- LIGGGHTS scales the point by `L_actual` and adds the particle centre.
- Point definition to agree on. Our stopgap uses the penetration-weighted mean of the penetrating
  vertices, projected onto the plane; the centroid of the penetrating region is an alternative. It must vary **continuously** with
  orientation (no jumps when switching between corner, edge and face contact), because a jump
  becomes a torque spike.
- For option4 an exact part exists (the support point of `h(q)`), analogous to `h_exact`; a
  learned correction could follow the same residual scheme.
- Please state the expected accuracy and include reference rows (`n_ref`) so we can verify the C++
  path like we did for `sd`.
- A new `scalers.dat` format version or field (e.g. `outputs sd,cp`) so LIGGGHTS can detect it.

## Contact

Please reply with the planned output definition and timeline; we will specify the LIGGGHTS side
(Spec 00 R00.2 output extension, Spec 04 R04.2b) against it.
