# Spec 04 — Geometric Contact Models (Surrogate-Based)

**Status:** Approved — revised 2026-11-17 (input/output contract corrected to run_029; see Spec 00 revision note)

## Context and Objective

Define how surrogates predict geometric contact quantities (overlap, contact point, normal) and how these are integrated with physics models. The surrogate is the ONLY source of geometric contact data; no mixing with native algorithms.

## Requirements

### R04.1: Scope (Geometric vs. Physics, HARD BOUNDARY)

**Geometric Contact (Surrogate Responsibility):**
- Overlap (signed distance between shape and wall)
- Contact point (where shape touches wall)
- Contact normal (direction perpendicular to wall surface)

**Physics Contact (Unchanged):**
- Force model (Hertz, Hooke, JKR, etc.)
- Friction (Coulomb, rolling resistance, etc.)
- Damping (velocity-dependent energy dissipation)
- Cohesion (adhesive forces)

**Hard Rule:** Surrogate predicts overlap, contact point, normal TOGETHER from a single query. These three come from the SAME geometric evaluation. Do NOT mix:
- ✗ Surrogate overlap + native superquadric contact point
- ✗ Surrogate overlap + post-hoc contact point calculation
- ✗ Different models for different outputs

**Enforcement in Code:**

```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag) {
    // Query surrogate ONCE
    auto surrogate_output = model->forward(input_tensor);
    
    // Extract ALL geometric outputs from single call
    overlap = denormalize(surrogate_output);   // Spec 00 R00.3
    contact_point = surrogate_output.contact_point;  // future
    normal = surrogate_output.normal;                 // future
    
    // Pass to physics model
    double force = physics_model->compute_force(overlap, contact_point, normal, ...);
}
#endif
```

### R04.2: ss-wall Contact (Now - Phase 3C)

**Target:** Surrogate cube vs plane (primitive wall)

**Input to Surrogate (Spec 00 R00.1):**
- `z_unit = centre_distance / L_actual`, where `centre_distance` = distance from the cube
  centre to the wall plane. For a primitive wall `resolveContact` returns
  `deltan = centre_distance - radius`, hence `centre_distance = deltan + radius` (radius = L/2).
- `quat = [qw, qx, qy, qz]` (particle orientation, canonicalized, unit norm), float32

**Output from Surrogate:**
- Scaled residual `r_s` (current; only output) → `sd_unit = r_s*y_scale + y_mean - h_exact(q, d)`
  with `d` = unit vector centre → wall (from `delta`). Contact point / normal: future (Phase 3D)

**Contact Calculation Flow:**

1. **Prefilter:** IF `z_unit > z_prefilter` → no contact; hard-zero history; skip
2. **Prepare Input:** canonicalize + normalize quaternion; `z_unit`
3. **Query Surrogate:** `raw = model.forward(position_z, quat)`
4. **Denormalize:** `sd_unit = raw*y_scale + y_mean - h_exact(q, d)` (Spec 00 R00.3)
5. **Rescale:** `sd_phys = sd_unit * L_actual`; `deltan = sd_phys`
6. **Validate:** NaN/Inf → error; `sd_unit < sd_too_deep` → warning (extrapolation)
7. **Physics Integration:** IF `deltan <= 0`, pass `-deltan` (penetration) to the force model

### R04.2b: Contact Point for Torque (MANDATORY, open — recorded 2026-11-17)

**Status:** Not implemented. Required before results with tilted or rotating cubes can be trusted.

**Contact normal (no action needed):** for a cube against a plane, the force direction is
the **wall normal**, for face, edge and corner contacts alike. LIGGGHTS already has it
(`delta` from the primitive wall). Only for future particle–particle (ss-ss) contact can
the normal depend on the particle orientation (e.g. a face normal of one cube).

**Contact point (missing):** the force must act at the touching feature (corner, edge
midpoint or face centre), not through the particle centre. The lever arm `r = x_c − x_centre`
gives the torque `τ = r × F` for both the normal force and the friction force.

Current behaviour (Phase 3C): `sidata.is_non_spherical` is false for surrogate particles, so
- normal force: no torque (acts through the centre);
- friction force: torque with the sphere lever arm `cri ≈ L/2` along the normal.

Lever arm relative to the centre, per unit L (cube resting on a plane, computed 2026-11-17):

| Orientation | Touching vertices | In-plane lever arm | Normal-force torque |
|---|---|---|---|
| flat face | 4 | 0 | 0 |
| edge, exactly 45° | 2 | 0 | 0 |
| corner, body diagonal vertical | 1 | 0 | 0 |
| edge, 30° about x | 2 | 0.183 | ≠ 0 |
| corner, generic tilt q = (0.9, 0.3, 0.2, 0.1) | 1 | 0.259 | ≠ 0 |

Symmetric orientations give zero torque; every generic tilt does not. A tilted cube therefore
cannot tip over, and normal impact cannot change its spin, until this is implemented.

**Constitution rule 1:** the contact point must come from the same geometric evaluation as
the overlap. The run_029 model outputs only `sd`. Therefore:
- RF-8: THE SYSTEM SHALL obtain the wall contact point from the surrogate evaluation that
  produces the overlap (model output, or the exact part of the same option4 evaluation if
  approved), and SHALL set `sidata.contact_point` / `is_non_spherical` so that the native
  normal and tangential models apply `τ = r × F`.
- Request to the surrogate (training) team: add the contact point to the model output —
  `DOCUMENTATION.md/REQUEST_SURROGATE_TEAM_CONTACT_POINT_20261117.md`.

### R04.3: Input Validation (CRITICAL - Prevents Silent Failures)

**Before Surrogate Query:**
- IF z_unit is NaN or Inf, error: "Invalid z_unit (NaN/Inf) at particle <id>"
- IF quaternion contains NaN or Inf, error: "Invalid quaternion at particle <id>"
- IF characteristic_length <= 0, error: "Invalid characteristic_length at particle <id>"
- IF z_unit > z_prefilter, the model is not called (no contact possible)

**After Surrogate Query:**
- IF model output is NaN or Inf, error: "Surrogate inference produced NaN/Inf at particle <id> with z_unit=<>, quat=<>"
- IF denormalized overlap is NaN or Inf, error: "Overflow/underflow in denormalization at particle <id>"

**After Rescaling:**
- IF overlap_phys is physically impossible (positive for true penetration case, see R04.4), warn/error based on severity

### R04.4: Physical Validity Check

**Current Signed Distance Convention:**
- overlap ≤ 0: Contact (penetration)
- overlap > 0: Gap (no contact)

**Rule:** IF surrogate predicts overlap > 0 (gap), do NOT compute contact force (skip this contact).

**Special Case (Future ss-ss Contact):**
- overlap can be positive (particles far apart) or negative (overlapping)
- Surrogate determines contact; trust output

### R04.5: Model Identity Validation (Prevents Shape Mismatches)

**At Runtime (before querying model):**

```cpp
// Verify shape matches model
int particle_shape_key = atom->shape_key[i];
std::string particle_shape = surrogate_shape_names[particle_shape_key];

std::string loaded_model_type = scaler_metadata["model_type"];  // "option4" or "option1"
std::string loaded_shape = scaler_metadata["run_id"];           // "run_029", "run_030", etc.

if (particle_shape != loaded_shape_inferred_from_model) {
    error->all(FLERR,
        fmt::format("Shape mismatch: particle shape_key={} ({}), model is for {}",
                   particle_shape_key, particle_shape, loaded_shape));
}
```

### R04.6: Contact History Management

**Hard-Zeroing (Replicate Superquadric Pattern):**

When contact breaks (deltan > 0) or the particle is prefiltered out:

```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag && deltan > 0.0) {
    if(c_history)
        vectorZeroizeN(c_history[iPart], dnum_);
    continue;  // next particle (NOT break: break would skip all remaining particles)
}
#endif
```

**Rationale:**
- Once overlap becomes positive (gap), contact is broken
- Zero historical state (friction, damping, adhesion)
- Restart fresh if particles come back into contact

**Energy/Momentum Implications:**
- Zeroing history can cause discontinuities in force
- Acceptable for contact breaking (physical behavior)
- Test for energy conservation before/after breaks

### R04.7: ss-ss Contact (Future - Phase 3D, Exploratory)

**Model Training:** Each (shape_i, shape_j) pair gets separate trained model
- Example: ss-cube vs ss-cube model
- Example: ss-cube vs ss-pyramid model (asymmetric)

**Model Selection at Runtime:**
- Lookup by both shape_keys: model = database[shape_key_i][shape_key_j]

**Output:** Same pattern as ss-wall (overlap, contact point, normal)

**Status:** Exploratory; no prior art; design deferred pending Phase 3C validation

### R04.8: Mesh Walls (Future)

**Currently:** Surrogate models only primitive walls (planes, cylinders)

**Future:** May extend to mesh walls via separate shape type (not in Phase 3C)

### R04.9: Pair Style and Cutoff Control

**Explicit Pair Style:**
- Surrogate contact uses explicit `pair_style` selection (LIGGGHTS convention)
- User specifies: `pair_style surrogate <cutoff>`
- Without this, surrogate contact is NOT active (safe: defaults to native algorithm)

**Runtime Warning:**
- IF input declares `atom_style surrogate ss-cube` BUT no matching pair_style is active, warn:
  "Surrogate particles present but no surrogate pair_style active; contact will use native algorithm"

### R04.10: Error Handling and Diagnostics

**Contact Calculation Failures:**
- IF surrogate model inference fails (exception), error with: "Model inference failed at particle <id>, step <step>, attempting shape <shape>, values <z, q>"
- IF physics model computation fails, error with: "Physics model failed after surrogate; force overflow or NaN"

**Diagnostic Output (optional logging):**
- Log surrogate queries (z_unit, quat, output) for debugging
- Log model selection (which model loaded, which particles use it)
- Log contact breaks (when zeroing history)

**Runtime Assertions (optional, Phase 3D+):**
- Assert overlap is denormalized correctly (round-trip test in debug mode)
- Assert quaternion remains canonicalized throughout simulation

---

## Definitions

| Term | Definition |
|------|-----------|
| **sd_unit** | Signed distance for the unit cube (Spec 00) |
| **sd_phys** | Physical signed distance = sd_unit · L_actual (assigned to deltan) |
| **z_unit** | Cube centre distance to the wall / L_actual |
| **deltan** | LIGGGHTS variable: normal displacement (negative for penetration) |
| **c_history** | Contact history array (friction, damping state) |
| **shape_key** | Integer index for shape identity (0 for ss-cube, 1 for ss-pyramid, etc.) |

---

## Functional Requirements

- RF-1: THE SYSTEM SHALL query surrogate model with validated input
- RF-2: THE SYSTEM SHALL denormalize and rescale output correctly
- RF-3: WHEN surrogate output is invalid (NaN/Inf), THE SYSTEM SHALL error with diagnostic
- RF-4: WHEN particle shape does not match model, THE SYSTEM SHALL error with diagnostic
- RF-5: THE SYSTEM SHALL hard-zero contact history when deltan > 0 (contact breaks)
- RF-6: THE SYSTEM SHALL ensure all geometric outputs (overlap, point, normal) come from single surrogate call
- RF-7: THE SYSTEM SHALL support explicit pair_style selection

---

## Non-Functional Requirements

- NFR-1: Contact calculation must not add >5% overhead compared to native algorithms
- NFR-2: Error messages must be diagnostic and actionable
- NFR-3: Physics boundary enforcement must be enforced at compile time and runtime

---

## Edge Cases

- EC-1: WHEN sd_unit < sd_too_deep (penetration deeper than training data), proceed but warn
- EC-2: WHEN contact between particles (ss-ss, not available yet), handle gracefully (error with suggestion)
- EC-3: WHEN z_unit > z_prefilter, the surrogate is not evaluated
- EC-4: A tilted cube can touch the wall with its centre up to r_circ·L = 0.866 L away. Primitive-wall neighbour detection is sphere-based (radius L/2 + skin); a corner contact may be detected late. [NEEDS CLARIFICATION — open, does not affect face-down tests]

---

## Completion Criteria

- CC-1: Input validation prevents NaN/Inf from reaching model
- CC-2: Model output validation prevents invalid contact forces
- CC-3: Model identity check prevents shape mismatches
- CC-4: Contact history is hard-zeroed on break
- CC-5: Physics/geometry boundary is explicitly enforced in code
- CC-6: All geometric outputs come from single surrogate evaluation

---

## Phase 3C Design Decisions (2026-11-10 Finalized)

### Wall Normal Direction (Generalized for Any Wall Orientation)

`primitiveWall_->resolveContact(x, r, delta)` returns `deltan = |centre - wall| - r` and the
vector `delta` from the centre toward the wall. The surrogate uses:
- `centre_distance = deltan + r` → `z_unit = centre_distance / L_actual`
- `d = delta / |delta|` → direction for `h_exact(q, d)` (generalizes the training floor `d = -e_z`)

> Corrected 2026-11-17: the former `s_unit = |deltan| / L_actual` definition was wrong — the
> model input is the centre distance, not the gap.

### Contact Cutoff Strategy (Follow Superquadric Pattern)

1. Call `primitiveWall_->resolveContact()` for fast sphere-based distance computation
2. If `deltan <= cutoff` (close enough), query surrogate model
3. Use surrogate overlap output directly (no analytical refinement per Spec 04 R04.1)
4. Skip the model if `z_unit > z_prefilter` (scalers.dat; 1.0 for run_029)

**Note:** `z_prefilter` is shape-specific and comes with the model.

### Shape Mismatch Detection (Phase 3C Assumption)

**Current approach:** Trust folder structure. Model shape is inferred from path: `/SS_models/{shape_name}-walls/model.pt`.

**Phase 3C Limitation:** No explicit shape metadata field in scalers.dat file. Cannot distinguish between models trained for different shapes at runtime.

**Phase 3D+ Roadmap:** Add explicit `shape_target` field to scalers.dat (ss_model_record_v2 or extended v1) to enable robust multi-shape validation.

### Input Normalization (`z_unit` Definition)

`z_unit = (deltan + radius) / L_actual` = cube centre distance to the wall / L_actual
(Spec 00 R00.1). Mapping to the model: `forward(position_z=[z_unit], quat=[[qw,qx,qy,qz]])`.

### Validation Strategy (Phase 3C - Session 4 Decision)

**Approach: Lightweight Hybrid Validation**

Phase 3C validation focuses on detecting major bugs without extensive analytical derivation. All checks are wrapped in `#ifdef SURROGATE_VALIDATION` pragma for easy toggle.

**Test Case Design:**
- Single cube: L=1.0 m (trained scale, scaling_factor=1.0)
- Free fall from z=10 m under gravity
- Wall contact at z=0 (plane, zplane primitive)
- Duration: 2 seconds (enough for contact, bounce, settle)
- Metrics: Contact force, energy conservation, z_unit / sd_unit range

**Validation Checks:**

1. **Contact Force Sanity (Covers R04.1, R04.3, R04.6)**
   - Assertion: During overlap (deltan < 0), normal_force > 0
   - Rationale: Catches sign errors, model loading failures, physics model integration bugs
   - Output: Warning if force ≤ 0 during contact

2. **Gap Behavior (Covers R04.6)**
   - Assertion: When gap opens (deltan > 0), contact history is hard-zeroed
   - Rationale: Validates hard-zeroing logic, prevents stale friction state
   - Output: Warning if history non-zero after contact break

3. **Energy Conservation (Covers R04.5)**
   - Metric: ΔE_step / E_total < 5% per timestep
   - Metric: ΔE_total / E_initial < 5% over full simulation
   - Rationale: Detects dissipation bugs, integration errors, force sign errors
   - Output: Step-by-step energy log; warning if loss exceeds threshold

4. **Numerical Safety (Covers R04.3)**
   - Assertion: No NaN/Inf in sd, force, z_unit, quaternion
   - Rationale: Catches denormalization formula errors, model failures
   - Output: Hard error if NaN/Inf detected

5. **Training Range Check (Covers R04.4, EC-1)** — implemented unconditionally (not behind the pragma)
   - z_unit > z_prefilter → model skipped; sd_unit < sd_too_deep (-0.1) → warning

**Success Criteria:**
- All contact forces > 0 during overlap
- Total energy loss < 5% over full simulation
- History properly zeroed on gap
- No NaN/Inf values
- sd_unit stays above sd_too_deep (or logged as extrapolation)

**Output Diagnostics (one line per contact timestep):**
```
Step | PID | Overlap(m) | Force(N) | z_unit | KE(J)  | PE(J)  | ΔE(J)  | Checks
 100 | 1   | 0.0150    | 2534    | 0.0150 | 45.23  | 120.34 | -0.12  | OK✓
 200 | 1   | 0.0089    | 1342    | 0.0089 | 32.15  | 105.67 | 0.08   | OK✓
 300 | 1   | 0.0000    | 0       | 0.0000 | 18.92  | 95.23  | -0.05  | GAP (history cleared)
```

**NOT Included (Deferred to Spec 05 Phase 3D+):**
- Superquadric comparison (requires validating superquadric as reference)
- Analytical Hertz solution (cube-plane contact is non-trivial)
- Multi-scale testing (different L_actual)
- Multi-shape validation (cube-cube, cube-cylinder)
- Advanced physics (friction, rolling, cohesion in contact)

**Pragmas (Implementation Pattern):**
```cpp
#ifdef SURROGATE_VALIDATION
  // Check 1: Force sanity
  if(deltan < 0.0 && normal_force <= 0.0) {
    char msg[256];
    snprintf(msg, 256, "Contact force sanity check FAILED at step %d, particle %d", step, id);
    error->warning(FLERR, msg);
  }
  
  // Check 2: Energy conservation
  double dE = (KE_new + PE_new) - (KE_old + PE_old);
  if(fabs(dE / E_total) > 0.05) {
    // Log warning + diagnostic
  }
  
  // ... remaining checks
#endif
```

**Transition Path:**
1. Implement all 5 checks with pragmas
2. Run test_01 (L=1.0 m cube falling)
3. Verify all checks pass
4. Document results in session report
5. Remove pragmas in final production build (or keep for debugging)
6. Spec 05 will extend to multi-scale, multi-shape, analytical validation

---

## Validation Results (2026-11-17, after contract correction)

Face-down cube dropped on a `zplane` wall; Hertz + tangential history, E = 1e9 Pa, ν = 0.25,
e = 0.9, μ = 0.2; mass set to ρL³ (ρ = 1000 kg/m³). Data from the VTK dumps (`post/`).

| Case | L | dt | Particle kept | e (measured) | max penetration / L | contact time |
|---|---|---|---|---|---|---|
| test_01c | 1 m | 1e-4 | yes | 0.899 | 0.0444 | ≈ 9.1 ms |
| test_02 (dense dump) | 0.01 m | 1e-5 | yes | 0.900 | 0.0070 | ≈ 0.160 ms |

Undamped Hertz sphere–plane estimate (R = L/2, Y* = E/(2(1-ν²))): penetration/L = 0.0464 and
0.0074, contact time 10.0 ms and 0.159 ms. Measured penetrations are slightly below the
undamped values, as expected with damping. No `sd_too_deep` warnings. test_01 and test_01b
(L = 1 m) also keep the particle and rebound.

Numerical verification only. The force law is the native Hertz model acting on the
surrogate overlap; for a face-down cube the surrogate overlap equals the exact geometric
overlap to ~1e-4 L. No physical/experimental validation has been performed.
