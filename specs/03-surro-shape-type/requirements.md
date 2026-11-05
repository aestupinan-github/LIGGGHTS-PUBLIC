# Spec 03 — Surrogate Shape Type

**Status:** Approved (QA-resolved 2026-11-05)

## Context and Objective

Define the new `atom_style surrogate` particle shape type, which represents particles whose contact with walls is computed by trained ML surrogates instead of native geometric algorithms. Parallels superquadric pattern for consistency and maintainability.

## Requirements

### R03.1: New Atom Style Registration

- **Style Name:** `atom_style surrogate`
- **Description:** Particles with position, orientation (quaternion), characteristic length, and shape identity
- **Registration:** Follow `AtomStyle(surrogate, AtomVecSurrogate)` macro pattern
- **Per-Particle Data:**
  - `atom->x[3]` (position, existing)
  - `atom->quaternion[4]` (orientation, existing or new per-particle array)
  - `atom->characteristic_length` (double, new per-particle array) — size descriptor (e.g., cube edge length)
  - `atom->shape_key` (int, new per-particle array) — index into global shape database
  - `atom->surrogate_flag` (int, global flag in atom.h) — set to 1 for this style, 0 otherwise

### R03.2: Shape Database (Global)

**Structure:**
```cpp
// Global shape registry (declared in atom.h or shape_manager.h)
std::vector<std::string> surrogate_shape_names;   // e.g., ["ss-cube", "ss-pyramid"]
std::vector<int> surrogate_shape_keys;            // e.g., [0, 1]
```

**Initialization:** At `init()` time, when `atom_style surrogate` is declared in input file:
- Parse input file for shape declarations (e.g., `atom_style surrogate ss-cube`)
- Assign shape_key = index in database
- Store shape name for model lookup (see Spec 02)

**Input File Syntax (NEW - SPECIFIED HERE):**
```
atom_style surrogate ss-cube
```

**Parsing:** Input processor extracts "ss-cube" and:
1. Checks if already registered (error if duplicate)
2. Assigns shape_key = current database size
3. Appends to surrogate_shape_names and surrogate_shape_keys
4. Loads model for ss-cube from SS_models/ss-cube-walls/

**Constraints:**
- One shape type per simulation (runtime exclusive: cannot mix ss-cube and ss-pyramid in same run)
- Shape name must match a directory in SS_models/ (e.g., ss-cube → SS_models/ss-cube-walls/)
- IF shape directory not found, error: "Shape ss-cube not found in SS_models/; available shapes: [list]"

### R03.3: Characteristic Length per Particle

**Definition:** L_actual for each particle (used in nondimensionalization, Spec 00)

**Storage:** `atom->characteristic_length[i]` (double, per-particle array)

**Initialization:** Input file provides one of:
- Option A: Per-particle via `create_atoms` command (future)
- Option B: Global default via `pair_coeff` or config (Phase 3D)
- Option C: User-specified in fix or command

**Constraint:** L_actual > 0. IF L_actual ≤ 0, error: "characteristic_length must be > 0 for particle <id>"

### R03.4: Dual-Guard Pattern (MANDATORY)

**Compile-Time Guard:** `#ifdef SURROGATE_ACTIVE_FLAG`
**Runtime Guard:** `if(atom->surrogate_flag)`

**Rule:** BOTH must be used together in all surrogate-specific code:

```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag) {
    // Surrogate-specific code here
}
#endif
```

**Rationale:** 
- `#ifdef` alone: segfaults if code runs on non-surrogate atom styles (e.g., `atom_style multisphere` without SURROGATE_ACTIVE_FLAG defined)
- Flag alone: inefficient (branch taken every contact even if style is not surrogate)
- Combined: efficient + safe

**Locations:** All code that touches surrogate data structures or calls surrogate models:
- fix_wall_gran.cpp (contact calculation)
- pair_gran.cpp (ss-ss contact, future)
- force.cpp (force accumulation)
- dump command (when outputting surrogate data)

### R03.5: Compile-Time Selectability

**CMake Option:** `-DENABLE_SURROGATE=ON|OFF` (default: OFF)

**Effect:**
- `ON`: Compile surrogate atom style; define `SURROGATE_ACTIVE_FLAG`; register atom style
- `OFF`: Exclude surrogate code; SURROGATE_ACTIVE_FLAG not defined; style not available

**Parallel:** Matches `-DENABLE_SQ=ON` for superquadrics (Spec 01)

### R03.6: Runtime Exclusivity (One Shape Type per Simulation)

**Constraint:** Cannot mix shape types in same simulation

**Enforcement:**
- At init() time, check how many shape types are registered
- IF > 1 (e.g., ss-cube AND ss-pyramid), error: "Only one surrogate shape type permitted per simulation; found: [ss-cube, ss-pyramid]"
- Allow multiple particles of same shape type (OK: particle1=ss-cube, particle2=ss-cube)

**Rationale:** Simplifies contact logic; each shape has one trained model (single SS_models/ss-cube-walls/ directory)

### R03.7: Shape Geometry Integration

**Training Time (SS_models_training repo):**
- Shape geometry baked into trained model (network knows cube structure)
- Wrapper generates shape vertices for postprocessing (optional, future)

**Runtime (LIGGGHTS):**
- Shape geometry NOT loaded from STL or file (not needed)
- Surrogate model is the source of truth for contact
- No geometric intersection tests required

**Design Rationale:** Robust; avoids STL parsing, mesh interpretation, coordinate system questions

### R03.8: Shape Database Error Handling

- IF input file declares unknown shape (not in SS_models/), error with diagnostic listing available shapes
- IF shape_key lookup fails at runtime (shape not found in database), error with diagnostic
- IF characteristic_length is invalid (≤ 0, NaN, Inf), error with diagnostic

---

## Definitions

| Term | Definition |
|------|-----------|
| **surrogate_flag** | Per-style flag in atom.h; 1 for atom_style surrogate, 0 otherwise |
| **shape_key** | Integer index into surrogate_shape_names database |
| **characteristic_length** | Per-particle size descriptor (e.g., cube edge length); used for rescaling |
| **SURROGATE_ACTIVE_FLAG** | Compile-time preprocessor flag; defined if -DENABLE_SURROGATE=ON |
| **ss-cube-walls** | Shape directory name; contains model.pt and scalers.dat for cube-wall interaction |

---

## Functional Requirements

- RF-1: THE SYSTEM SHALL register atom_style surrogate with per-particle data arrays
- RF-2: THE SYSTEM SHALL parse `atom_style surrogate ss-cube` from input file and populate shape database
- RF-3: THE SYSTEM SHALL enforce one shape type per simulation (error if > 1 declared)
- RF-4: THE SYSTEM SHALL apply dual-guard pattern in all surrogate-specific code
- RF-5: WHEN characteristic_length is invalid, THE SYSTEM SHALL error with diagnostic
- RF-6: THE SYSTEM SHALL use shape_key to look up loaded model at runtime
- RF-7: THE SYSTEM SHALL match particle shape_key to model type during contact calculation

---

## Non-Functional Requirements

- NFR-1: Shape database lookup must be O(1) (array index, not map)
- NFR-2: Dual-guard pattern must add negligible overhead
- NFR-3: Error messages must list available shapes and suggest fixes

---

## Edge Cases

- EC-1: WHEN input file has no shape declaration, error: "atom_style surrogate requires shape name (e.g., `atom_style surrogate ss-cube`)"
- EC-2: WHEN shape directory does not exist in SS_models/, error with full path and available shapes list
- EC-3: WHEN two particles have different characteristic_length, both should work correctly (rescaling by individual lengths)

---

## Completion Criteria

- CC-1: atom_style surrogate is registered and compiles
- CC-2: Shape database is populated from input file syntax
- CC-3: One shape type per simulation rule is enforced
- CC-4: Dual-guard pattern is applied consistently across code
- CC-5: characteristic_length is stored and accessible per-particle
