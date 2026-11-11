# Spec 04 — Geometric Contact Models (Surrogate-Based)

**Status:** Approved (QA-resolved 2026-11-05)

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
    overlap = denormalize(surrogate_output.overlap_log10);
    contact_point = surrogate_output.contact_point;  // future
    normal = surrogate_output.normal;                 // future
    
    // Pass to physics model
    double force = physics_model->compute_force(overlap, contact_point, normal, ...);
}
#endif
```

### R04.2: ss-wall Contact (Now - Phase 3C)

**Target:** Surrogate cube vs plane (primitive wall)

**Input to Surrogate:**
- `z_unit = (z_particle - z_wall) / L_actual` (height above wall, normalized)
- `quat = [qw, qx, qy, qz]` (particle orientation, canonicalized)

**Output from Surrogate:**
- `log10(overlap_unit)` (current; only output)
- Contact point (future, Phase 3D)
- Contact normal (future, Phase 3D)

**Contact Calculation Flow:**

1. **Prepare Input:** Canonicalize quaternion, normalize height
2. **Query Surrogate:** Call model.forward([z_unit, quat])
3. **Denormalize:** overlap_unit = 10^(output) - eps_small (Spec 00)
4. **Rescale:** overlap_phys = overlap_unit * L_actual
5. **Validate:** Check for NaN/Inf, physically invalid values
6. **Physics Integration:** Pass overlap to force model

### R04.3: Input Validation (CRITICAL - Prevents Silent Failures)

**Before Surrogate Query:**
- IF z_unit is NaN or Inf, error: "Invalid z_unit (NaN/Inf) at particle <id>"
- IF quaternion contains NaN or Inf, error: "Invalid quaternion at particle <id>"
- IF characteristic_length <= 0, error: "Invalid characteristic_length at particle <id>"
- IF z_unit is outside training range (< -10 or > +10), warn (optional) but proceed (extrapolation)

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

When contact breaks (deltan > 0):

```cpp
#ifdef SURROGATE_ACTIVE_FLAG
if(atom->surrogate_flag && deltan > 0.0) {
    if(c_history)
        vectorZeroizeN(c_history[iPart], dnum_);
    break;  // Exit contact loop for this particle
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
| **overlap_unit** | Normalized overlap (model output domain, relative to L=1) |
| **overlap_phys** | Physical overlap in simulation units (signed distance) |
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

- EC-1: WHEN surrogate output is outside training range (far extrapolation), proceed but log warning
- EC-2: WHEN contact between particles (ss-ss, not available yet), handle gracefully (error with suggestion)
- EC-3: WHEN particle approaches from far away (z_unit >> training range), surrogate still evaluates (extrapolation acceptable)

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

**Key Finding:** The `delta` vector returned by `primitiveWall_->resolveContact(double *x, double r, double *delta)` already encodes the contact normal direction (pointing from wall toward particle).

**Implementation:** Instead of hardcoding z-axis assumption:
- Compute `s_unit = |deltan| / L_actual` where `deltan = ||delta||`
- This automatically handles walls in any orientation (xy-plane, yz-plane, xz-plane, cylinders, etc.)
- Replaces the original training assumption of z-axis walls

**Rationale:** The `delta` vector is already computed by `primitiveWall_` for sphere-based contact detection. Reusing it for surrogate input ensures consistency and generalization.

### Contact Cutoff Strategy (Follow Superquadric Pattern)

1. Call `primitiveWall_->resolveContact()` for fast sphere-based distance computation
2. If `deltan <= cutoff` (close enough), query surrogate model
3. Use surrogate overlap output directly (no analytical refinement per Spec 04 R04.1)
4. Skip contact if `s_unit > 3 * L_actual` (too far; no physical contact possible)

**Note on Elongated Shapes:** The `3 * L_actual` threshold assumes cubic-like shapes. For elongated shapes (future Phase 3D+), this may be too conservative or too permissive. Recommendation: make threshold shape-aware when multi-shape support is added.

### Shape Mismatch Detection (Phase 3C Assumption)

**Current approach:** Trust folder structure. Model shape is inferred from path: `/SS_models/{shape_name}-walls/model.pt`.

**Phase 3C Limitation:** No explicit shape metadata field in scalers.dat file. Cannot distinguish between models trained for different shapes at runtime.

**Phase 3D+ Roadmap:** Add explicit `shape_target` field to scalers.dat (ss_model_record_v2 or extended v1) to enable robust multi-shape validation.

### Input Normalization (`s_unit` Definition)

**Original (z-axis only):** `z_unit = (z_particle - z_wall) / L_actual`

**Generalized (any wall):** `s_unit = |deltan| / L_actual` where `deltan` comes from `primitiveWall_->resolveContact()`

**Mapping to Model Input Tensor:** `[s_unit, qw, qx, qy, qz]` (same 5-feature format as training)

### Validation Strategy (Phase 3C - Session 4 Decision)

**Approach: Lightweight Hybrid Validation**

Phase 3C validation focuses on detecting major bugs without extensive analytical derivation. All checks are wrapped in `#ifdef SURROGATE_VALIDATION` pragma for easy toggle.

**Test Case Design:**
- Single cube: L=1.0 m (trained scale, scaling_factor=1.0)
- Free fall from z=10 m under gravity
- Wall contact at z=0 (plane, zplane primitive)
- Duration: 2 seconds (enough for contact, bounce, settle)
- Metrics: Contact force, energy conservation, s_unit range

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
   - Assertion: No NaN/Inf in overlap, force, s_unit, quaternion
   - Rationale: Catches denormalization formula errors, model failures
   - Output: Hard error if NaN/Inf detected

5. **Training Range Check (Covers R04.4, EC-1)**
   - Assertion: During contact, s_unit ∈ [-0.5, 2.0] (model training bounds)
   - Rationale: Warns when extrapolating; helps calibrate skip threshold
   - Output: Warning if s_unit outside range (does not skip contact, but flags it)

**Success Criteria:**
- All contact forces > 0 during overlap
- Total energy loss < 5% over full simulation
- History properly zeroed on gap
- No NaN/Inf values
- s_unit stays within training range (or logged as extrapolation)

**Output Diagnostics (one line per contact timestep):**
```
Step | PID | Overlap(m) | Force(N) | s_unit | KE(J)  | PE(J)  | ΔE(J)  | Checks
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
