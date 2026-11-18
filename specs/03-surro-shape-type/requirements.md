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

**Initialization (decided 2026-11-17, radius revised 2026-11-18 → R03.11):** `L_actual` = particle **diameter**.
- `set atom|type|group ... diameter D` sets `characteristic_length = D` and `radius = r_circ·D`
  (bounding radius, R03.11; it was D/2 until 2026-11-18).
- Default for newly created particles (`create_atoms`, `read_data`): `L_actual = 1.0`,
  `radius = r_circ`, mass = density · k_V.
- Rationale: the unit cube (L=1) is the nondimensional training reference; each particle is
  mapped onto it by its own `L_actual` (Constitution rule 9). `radius = L_actual/2` is also
  what Spec 04 uses to recover the centre distance from the primitive-wall `deltan`.
- Since 2026-11-18 (R03.11) `set ... density` / `diameter` give the cube mass ρ·D³ directly; an
  explicit `set ... mass` (as in the existing test inputs) is still allowed and gives the same value.

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

### R03.6: Runtime Exclusivity (One Shape Type per Simulation) — DEPRECATED, see R03.6a

**[OUTDATED — See R03.6a below for current design]**

### R03.6a: Multiple Shape Types per Simulation (Future-Ready Design)

**Current Phase (3C):** Allow one shape type per simulation for simplicity  
**Future Phase (3D+):** Architecture shall support multiple shape types (cube-wall, cube-cube, pyramid-wall, etc.)

**Design Principle:** 
- Shape database is indexed by `shape_key` (integer)
- Each shape_key maps to a trained model directory (e.g., ss-cube-walls/, ss-cube-cube/, ss-pyramid-walls/)
- Multiple particles can have different shape_keys in same simulation
- Contact logic will dispatch to correct model based on particle pair's shape_keys

**Current Enforcement (Phase 3C):**
- At init() time, enforce maximum 1 registered shape type (error if > 1)
- Rationale: Simplifies initial deployment; one model directory per simulation run
- Code comment: "TODO: Phase 3D will lift this restriction to allow cube-to-cube + cube-to-wall in same run"

**Future Relaxation (Phase 3D+):**
- Remove one-shape-per-simulation check in `AtomVecSurrogate::init()`
- Add cross-shape contact model dispatch (Spec 04 T4.x)
- Example: particles with shape_key=0 (ss-cube-walls) contact particles with shape_key=1 (ss-cube-cube)
- Will require model registry: map {(shape_key_i, shape_key_j) → model_directory}

**Preparation for Future:**
- Shape database already stores vectors of names and keys (not single-shape array)
- `register_shape()` method accepts arbitrary names (no hardcoding of shape_names)
- Shape_key assignment is dynamic (counter-based), not enum-based
- Model path lookup uses shape_name, not hard-coded directories

**Edge Cases to Consider (Future):**
- Asymmetric contacts: Does ss-cube-to-wall model work for wall-to-ss-cube? (Need separate models or symmetry check)
- Many shapes (N > 5): O(N²) contact model combinations; may need model fallback strategy
- Load-time validation: Check that all required {shape_key_i, shape_key_j} models exist before first contact

---

**Old R03.6 (Archive):**

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

### R03.9: Multi-Shape Readiness (audit 2026-11-17)

**Rule for all new surrogate code:** shape-dependent quantities (support height, contact point,
bounding radius, inertia, volume) SHALL be obtained through the per-shape geometry layer
(`SurrogateShape`, Spec 04 design), never hard-coded as cube formulas at the call site.

Known single-shape / cube-specific points (to be resolved with multi-shape support, Phase 3D):

| Location | Limitation | Status |
|---|---|---|
| `atom_vec_surrogate.cpp` `create_atom` / `data_atom` | `shape_key = 0` for every particle; no input command sets it | open |
| `atom_vec_surrogate.cpp` `settings()` / `init()` | exactly one shape name; error if > 1 | open (R03.6a) |
| `fix_wall_gran.h` | one `SurrogateContactModel` per fix; `atom->shape_key[i]` never read | open — needs registry indexed by shape_key |
| `fix_wall_gran.cpp` init | loads `surrogate_shape_keys[0]` only | open |
| `surrogate_path_resolver.cpp` | probe hard-coded to `ss-cube-walls` | fixed 2026-11-17 (generic probe) |
| `surrogate_path_resolver.cpp` | `-walls` suffix only; no pair-model path (`<shapeA>-<shapeB>`) | open (R04.7) |
| cube geometry (`h_exact_cube`) called directly | no per-shape layer | fixed 2026-11-17 (`SurrogateShape`) |
| `set.cpp` `diameter` | L = 2r (cube inscribed sphere); mass recomputed as a sphere | open — per-shape L↔r, volume |
| `fix_nve_sphere.cpp` | isotropic inertia only | open for anisotropic shapes (body-frame integrator as `fix nve/superquadric`) |
| `atom_vec_surrogate.cpp` `pack_border*` | ghost atoms had no shape_key / L / radius; `size_border` (7) < packed (11) | fixed 2026-11-17 (15 values, periodic shift added) |
| `atom_vec_surrogate.cpp` `register_shape` | per-shape inertia factor | done 2026-11-17 (`surrogate_shape_inertia_factor[key]`, from scalers.dat `h_exact_method`) |
| restart (`read_restart` / `write_restart`) | not supported for surrogate | **open bug** — `.scratch/surrogate-restart/issues/01-restart-not-supported.md` |
| `ScalerRecord` | no explicit shape field (shape inferred from folder) | open (R04.5) |

### R03.10: Restart Files (KNOWN LIMITATION)

- `write_restart` / `read_restart` are **not supported** for `atom_style surrogate`
  (shape database not saved; `settings()` fails on restore; `size_restart` / `pack_restart`
  inconsistent). Bug report: `.scratch/surrogate-restart/issues/01-restart-not-supported.md`.

### R03.11: Particle Radius = Bounding Radius (2026-11-18)

- `atom->radius` of a surrogate particle SHALL be the bounding (circumscribed) radius
  `r_circ · L` of its shape (cube: 0.866 L), as for superquadrics. Neighbour bins, the pair cutoff,
  ghost cutoffs and primitive-wall neighbour lists then cover the full reach of the particle
  (corners), with LIGGGHTS default settings.
- `set ... diameter D` sets `characteristic_length = D`, `radius = r_circ · D`; `set ... density`
  and `set ... diameter` (with density) set `mass = ρ · k_V · L³` (cube: ρ L³), not a sphere mass.
- Contact code that needs the inscribed radius L/2 (Hertz `reff`, centre distance) uses
  `characteristic_length` explicitly (Spec 04).
- Rationale: with radius = L/2 the default bin size was 5× too small (150 ms vs 43 ms neighbour
  time for a single particle) and cube–cube corner contacts would be missed by the pair list.

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
- RF-8: WHEN `set ... diameter D` is applied to a surrogate particle, THE SYSTEM SHALL set characteristic_length = D (and radius = D/2)
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
- EC-4: Verified 2026-11-17: L=1 m and L=0.01 m cube drops give the same dimensionless response (Spec 04)

---

## Completion Criteria

- CC-1: atom_style surrogate is registered and compiles
- CC-2: Shape database is populated from input file syntax
- CC-3: One shape type per simulation rule is enforced (Phase 3C limitation; documented for future removal in Phase 3D+)
- CC-4: Dual-guard pattern is applied consistently across code
- CC-5: characteristic_length is stored and accessible per-particle
- CC-6: Shape database architecture supports multiple shapes (vectors not single-shape; dynamic key assignment; documented upgrade path)
