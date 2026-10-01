# CONSTITUTION.md — Surrogate Shape Project

Non-negotiable rules. Automatically interpreted and enforced at session start.

## Project Purpose

Add a new particle shape family — "surrogate" (`atom_style surrogate`) — whose wall/particle overlap is predicted by a trained ML surrogate (LibTorch), analogous to how superquadrics work. The surrogate replaces the **geometric** contact calculation (overlap, contact point, normal); physics models (Hertz, Hooke, etc.) remain unchanged.

## Branch Policy

- All work on `surrogate-shape-dev`, cut from `master`
- `ML_implementation` branch is discarded — do not inspect, merge, or touch

## Non-Negotiable Rules

1. **Consistency constraint**: Overlap AND contact point/normal MUST come from the same geometric evaluation. Never mix learned overlap with natively-computed contact point/lever-arm derived from a different source. This mismatch was identified as a likely cause of energy-gain/non-physical-bounce bugs in prior (discarded) work.

2. **Dual-guard pattern**: Both `#ifdef SURROGATE_ACTIVE_FLAG` AND runtime `if(atom->surrogate_flag)` must be used together. The `#ifdef` alone segfaults on non-surrogate atom styles (e.g., multisphere).

3. **Model loading at init()**: ML models loaded once at initialization, never per-contact. Per-contact loading would catastrophically slow simulations.

4. **Doxygen**: All new code must be Doxygen documented (`@brief`, `@param`, `@return`). Modifications to existing code must update/add Doxygen descriptions for the modified parts. Only files touched for ss/surrogate work need documentation updates.

5. **Build artifacts**: Never edit or propose edits to generated `style_*.h` / `style_contact_model.h` files. These are build artifacts, not source.

6. **Investigate before describing**: Verify actual source before describing how anything works. Do not guess at class names, member names, control flow, or build behavior.

7. **Stop and ask**: When something is ambiguous or uncertain, stop and ask rather than assuming or picking a default.

8. **Geometric vs. physics**: The surrogate replaces the **geometric** contact calculation (overlap, contact point, normal). **Physics** contact models (Hertz, Hooke, etc.) remain unchanged — they consume surrogate output. Do not confuse geometric and physics contact models.

9. **Nondimensionalization**: Train once at canonical L=1, rescale per-particle at query time by that particle's own characteristic length. This is a hard constraint — not open for debate.

10. **Quaternion canonicalization**: Enforce qw >= 0 (flip all components if qw < 0) to ensure consistent orientation representation.

## Authoritative State

- `MEMORY.md` — persistent memory across sessions (confirmed facts, open questions, decisions, status)
- `specs/` — requirements, design, tasks per topic
- `LOG.md` — session-by-session bitácora
