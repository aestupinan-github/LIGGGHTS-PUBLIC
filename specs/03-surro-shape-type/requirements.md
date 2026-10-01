# Spec 03 — Surrogate Shape Type

## Requirements

### New Atom Style
- `atom_style surrogate` — new particle shape family
- Follows the superquadric pattern (closest existing reference)
- Per-particle data: position, orientation (quaternion), characteristic length, shape key (int)

### Shape Database
- Global (follows LIGGGHTS convention — style registrations are global)
- Parallel vectors: `std::vector<std::string>` for names, `std::vector<int>` for keys
- Formed at init from input file shape names
- Multi-material support (Al vs Regolith) is future work at physics model level

### Dual-Guard Pattern
- Compile-time: `#ifdef SURROGATE_ACTIVE_FLAG`
- Runtime: `if(atom->surrogate_flag)`
- Both must be used together — the `#ifdef` alone segfaults on non-surrogate atom styles

### Coexistence
- Compile-time selectable (like superquadrics/multisphere)
- Runtime exclusive (one shape type per simulation, like current LIGGGHTS behavior)

### Shape Geometry
- Baked into surrogate model at training time
- Wrapper generates points for postprocessing
- STL files are training-time only — not needed at runtime in cpp
