/* ----------------------------------------------------------------------
   Surrogate Interface Contract Implementation
   Spec 00: Interface between ML models and LIGGGHTS contact calculation
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#ifndef LMP_SURROGATE_INTERFACE_H
#define LMP_SURROGATE_INTERFACE_H

#include <cmath>
#include <cstring>

namespace LAMMPS_NS {
namespace SurrogateInterface {

/* =====================================================================
   Constants
===================================================================== */

/// Half edge length of the canonical (L=1) cube used in training.
/// Matches compute_h_exact_batch() in the run_029 training code.
static constexpr double CUBE_HALF_EDGE_UNIT = 0.5;

/**
 * @brief Body-to-world rotation matrix R(q) for q = [qw, qx, qy, qz]
 *
 * Same convention as MathExtraLiggghtsNonspherical::quat_to_mat and the
 * run_029 training code (compute_h_exact_batch).
 *
 * @param quat Unit quaternion [qw, qx, qy, qz]
 * @param R    Output 3x3 rotation matrix (body -> world)
 */
inline void rotation_matrix(const double quat[4], double R[3][3])
{
  const double w = quat[0], x = quat[1], y = quat[2], z = quat[3];
  R[0][0] = 1 - 2*(y*y + z*z); R[0][1] = 2*(x*y - z*w);     R[0][2] = 2*(x*z + y*w);
  R[1][0] = 2*(x*y + z*w);     R[1][1] = 1 - 2*(x*x + z*z); R[1][2] = 2*(y*z - x*w);
  R[2][0] = 2*(x*z - y*w);     R[2][1] = 2*(y*z + x*w);     R[2][2] = 1 - 2*(x*x + y*y);
}

/* =====================================================================
   Exact support height (run_029 / option4 physics part)
===================================================================== */

/**
 * @brief Exact support height h(q) of the canonical cube toward a wall
 *
 * h = max over the 8 cube vertices v of  (R(q) v) . d
 * where R(q) is the body-to-world rotation matrix and d is the unit vector
 * pointing from the particle centre toward the wall.
 *
 * For a floor (d = -e_z) this is identical to the training formula
 * h(q) = max_v v . (-R^T e_z)  (run_029 training_code.py, compute_h_exact_batch),
 * i.e. the depth of the lowest vertex below the centre, in units of L.
 * Range: 0.5 (face toward wall) ... sqrt(3)/2 (vertex toward wall).
 *
 * Quaternion convention [qw,qx,qy,qz], body-to-world, same as
 * MathExtraLiggghtsNonspherical::quat_to_mat and the training code.
 *
 * @param quat     Unit quaternion [qw, qx, qy, qz]
 * @param dir_wall Unit vector from particle centre toward the wall (world frame)
 *
 * @return h_unit  Support height for the unit cube (dimensionless)
 */
inline double h_exact_cube(const double quat[4], const double dir_wall[3])
{
  double R[3][3];
  rotation_matrix(quat, R);
  // body-frame direction toward wall: u = R^T d
  double u[3];
  for (int k = 0; k < 3; k++)
    u[k] = R[0][k]*dir_wall[0] + R[1][k]*dir_wall[1] + R[2][k]*dir_wall[2];
  // max over cube vertices (+-a,+-a,+-a) of v.u = a*(|u0|+|u1|+|u2|)
  return CUBE_HALF_EDGE_UNIT * (std::fabs(u[0]) + std::fabs(u[1]) + std::fabs(u[2]));
}

/**
 * @brief STOPGAP (Spec 04 R04.2b): contact point of the unit cube on a plane
 *
 * TEMPORARY. The contact point MUST eventually come from the surrogate model
 * output (Constitution rule 1, single geometric evaluation). Until then it is
 * computed here from the exact cube geometry of the same option4 evaluation.
 * Cube only. See DOCUMENTATION.md/REQUEST_SURROGATE_TEAM_CONTACT_POINT_20261117.md
 *
 * Definition: with the penetration depth p = -sd_unit > 0 taken from the
 * surrogate, the wall plane sits at depth (h - p) below the centre along d.
 * Vertex k has depth s_k = v_k . d; it is in contact when s_k > h - p, with
 * weight w_k = s_k - (h - p). The deepest vertex (s = h) always has weight p,
 * so the point is defined even when the model's sd differs from h_exact by
 * ~1e-4. The point is the weighted mean of the contacting vertices, projected
 * onto the wall plane. It varies continuously corner -> edge -> face.
 *
 * @param quat      Unit quaternion [qw, qx, qy, qz]
 * @param dir_wall  Unit vector from particle centre toward the wall (world frame)
 * @param sd_unit   Signed distance from the surrogate (unit cube; < 0 = contact)
 * @param cp_unit   Output: contact point relative to the centre, world frame, units of L
 *
 * @return true if a contact point was computed (sd_unit < 0), false otherwise
 */
inline bool contact_point_cube(const double quat[4], const double dir_wall[3],
                               double sd_unit, double cp_unit[3])
{
  cp_unit[0] = cp_unit[1] = cp_unit[2] = 0.0;
  if (!(sd_unit < 0.0)) return false;

  double R[3][3];
  rotation_matrix(quat, R);
  const double a = CUBE_HALF_EDGE_UNIT;
  const double h = h_exact_cube(quat, dir_wall);
  const double p = -sd_unit;
  const double plane_depth = h - p;   // depth of the wall plane below the centre along d

  double wsum = 0.0, acc[3] = {0.0, 0.0, 0.0};
  for (int ix = -1; ix <= 1; ix += 2)
    for (int iy = -1; iy <= 1; iy += 2)
      for (int iz = -1; iz <= 1; iz += 2) {
        const double vb[3] = {ix*a, iy*a, iz*a};
        double vw[3];
        for (int r = 0; r < 3; r++)
          vw[r] = R[r][0]*vb[0] + R[r][1]*vb[1] + R[r][2]*vb[2];
        const double s = vw[0]*dir_wall[0] + vw[1]*dir_wall[1] + vw[2]*dir_wall[2];
        const double w = s - plane_depth;
        if (w > 0.0) {
          wsum += w;
          for (int r = 0; r < 3; r++) acc[r] += w * vw[r];
        }
      }
  if (wsum <= 0.0) return false;   // unreachable: deepest vertex has weight p > 0

  for (int r = 0; r < 3; r++) acc[r] /= wsum;
  // project onto the wall plane: remove the component along d, put it at plane_depth
  const double along = acc[0]*dir_wall[0] + acc[1]*dir_wall[1] + acc[2]*dir_wall[2];
  for (int r = 0; r < 3; r++)
    cp_unit[r] = acc[r] + (plane_depth - along) * dir_wall[r];
  return true;
}

/* =====================================================================
   Per-shape geometry layer (Spec 03 R03.9, Spec 04 design)
   All shape-dependent quantities go through this struct; new shapes add a
   new entry, never cube formulas at the call site.
===================================================================== */

/**
 * @brief Shape-dependent constants and geometry functions (unit size L = 1)
 */
struct ShapeGeometry {
  const char *name;            ///< identifier, matches scalers.dat h_exact_method
  double bounding_radius;      ///< circumscribed radius / L (corner reach)
  double inertia_factor;       ///< k_I in I = k_I m L^2 (isotropic shapes only)
  double volume_factor;        ///< k_V in V = k_V L^3
  /// exact support height toward the wall, units of L
  double (*support_height)(const double quat[4], const double dir_wall[3]);
  /// STOPGAP (Spec 04 R04.2b) contact point, units of L, relative to the centre
  bool (*contact_point)(const double quat[4], const double dir_wall[3],
                        double sd_unit, double cp_unit[3]);
};

/// Cube (run_029, h_exact_method analytical_rotation_matrix)
static const ShapeGeometry SHAPE_CUBE = {
  "analytical_rotation_matrix",
  0.86602540378443864676,      // sqrt(3)/2
  1.0 / 6.0,                   // solid cube: I = m L^2 / 6 about any centroidal axis
  1.0,
  &h_exact_cube,
  &contact_point_cube
};

/**
 * @brief Select the shape geometry from the scalers.dat h_exact_method field
 *
 * @param h_exact_method value of the h_exact_method field
 *
 * @return pointer to the shape geometry, or nullptr if unknown
 */
inline const ShapeGeometry *shape_geometry_for(const char *h_exact_method)
{
  if (h_exact_method && std::strcmp(h_exact_method, SHAPE_CUBE.name) == 0)
    return &SHAPE_CUBE;
  return nullptr;
}

/* =====================================================================
   Denormalization (Spec 00, run_029 option4 residual model)
===================================================================== */

/**
 * @brief Convert raw network output into the signed distance (unit cube)
 *
 * run_029 is trained on the StandardScaler-scaled residual  r = sd + h(q).
 * Inverse (training_code.py lines 368-371):
 *   residual = out * y_scale + y_mean
 *   sd_unit  = residual - h(q)
 *
 * Sign convention (same as LIGGGHTS deltan): sd > 0 gap, sd < 0 penetration.
 * There is NO log10 / eps in this model.
 *
 * @param raw_output Raw network output (scaled residual)
 * @param y_mean     Scaler mean from scalers.dat
 * @param y_scale    Scaler scale from scalers.dat
 * @param h_unit     Exact support height h(q), see h_exact_cube()
 *
 * @return sd_unit   Signed distance for the unit cube (dimensionless)
 */
inline double denormalize_signed_distance(double raw_output, double y_mean,
                                          double y_scale, double h_unit)
{
  return (raw_output * y_scale + y_mean) - h_unit;
}

/**
 * @brief Rescale a unit-cube length to physical units
 *
 * Formula: value_phys = value_unit * L_actual   (Constitution rule 9)
 *
 * @param value_unit Dimensionless length (canonical L=1)
 * @param L_actual   Particle characteristic length (cube edge)
 *
 * @return Physical length
 */
inline double rescale_to_physical(double value_unit, double L_actual)
{
  return value_unit * L_actual;
}

/* =====================================================================
   Quaternion Canonicalization (Spec 00, R00.6)
===================================================================== */

/**
 * @brief Canonicalize quaternion to qw >= 0
 * 
 * If qw < 0, flip all components: q = -q
 * This ensures deterministic representation (same quaternion always canonicalizes same way)
 * 
 * Spec 00, R00.6
 * 
 * @param quat Input quaternion [qw, qx, qy, qz]
 *             Modified in-place to canonical form
 * 
 * @note This is a silent normalization step (no warning)
 * @note Must be called before passing quaternion to surrogate model
 */
inline void canonicalize_quaternion(double quat[4])
{
  if (quat[0] < 0.0) {
    quat[0] = -quat[0];
    quat[1] = -quat[1];
    quat[2] = -quat[2];
    quat[3] = -quat[3];
  }
}

/**
 * @brief Check if quaternion is valid (no NaN or Inf components)
 * 
 * @param quat Input quaternion [qw, qx, qy, qz]
 * 
 * @return true if all components are finite; false if any NaN or Inf
 * 
 * @note Called as part of input validation (Spec 00, R04.3)
 */
inline bool is_quaternion_valid(const double quat[4])
{
  for (int i = 0; i < 4; i++) {
    if (!std::isfinite(quat[i])) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Normalize quaternion to unit magnitude
 * 
 * Formula: q_norm = q / ||q||
 * 
 * @param quat Input quaternion [qw, qx, qy, qz]
 *             Modified in-place to unit norm
 * 
 * @return Magnitude of input quaternion (before normalization)
 * 
 * @note Spec 00, R00.1 suggests quaternions should arrive normalized.
 *       This function is defensive (normalizes if needed).
 * @note If magnitude is very small (< 1e-15), returns 0 without modifying quat.
 */
inline double normalize_quaternion(double quat[4])
{
  double mag_sq = quat[0]*quat[0] + quat[1]*quat[1] + quat[2]*quat[2] + quat[3]*quat[3];
  double mag = std::sqrt(mag_sq);
  
  if (mag < 1e-15) {
    return 0.0;  // Degenerate; don't normalize
  }
  
  quat[0] /= mag;
  quat[1] /= mag;
  quat[2] /= mag;
  quat[3] /= mag;
  
  return mag;
}

/* =====================================================================
   Input Validation
===================================================================== */

/**
 * @brief Check if value is finite and in physically valid range
 * 
 * @param value Value to check
 * @param name  Name of value (for error messages)
 * 
 * @return true if finite; false if NaN or Inf
 */
inline bool is_finite(double value)
{
  return std::isfinite(value);
}

/**
 * @brief Check if characteristic length is valid
 * 
 * Constraint: L_actual > 0
 * 
 * @param L_actual Characteristic length
 * 
 * @return true if L_actual > 0; false otherwise
 */
inline bool is_characteristic_length_valid(double L_actual)
{
  return (L_actual > 0.0 && std::isfinite(L_actual));
}

/* =====================================================================
   Tensor Preparation (Spec 00, Input preparation)
===================================================================== */

/**
 * @brief Prepare input tensor for surrogate model
 * 
 * Input format: [z_unit, qw, qx, qy, qz] (5 features)
 * 
 * Steps:
 * 1. Normalize z: z_unit = z_phys / L_actual
 * 2. Canonicalize quaternion (qw >= 0)
 * 3. Pack into 5-element array
 * 
 * Spec 00, R00.1
 * 
 * @param z_phys           Particle center height relative to wall (physical units)
 * @param L_actual         Characteristic length (must be > 0)
 * @param quat_input       Input quaternion [qw, qx, qy, qz] (4 elements)
 * @param input_tensor     Output tensor [z_unit, qw, qx, qy, qz] (5 elements)
 * 
 * @note input_tensor must have size >= 5
 * @note Does NOT validate; caller must validate L_actual > 0 and quat_input validity
 */
inline void prepare_input_tensor(
  double z_phys,
  double L_actual,
  double quat_input[4],
  double input_tensor[5])
{
  // Normalize height
  double z_unit = z_phys / L_actual;
  input_tensor[0] = z_unit;
  
  // Canonicalize quaternion and pack
  double quat_canonical[4];
  std::memcpy(quat_canonical, quat_input, 4 * sizeof(double));
  canonicalize_quaternion(quat_canonical);
  
  input_tensor[1] = quat_canonical[0];  // qw
  input_tensor[2] = quat_canonical[1];  // qx
  input_tensor[3] = quat_canonical[2];  // qy
  input_tensor[4] = quat_canonical[3];  // qz
}

} // namespace SurrogateInterface
} // namespace LAMMPS_NS

#endif // LMP_SURROGATE_INTERFACE_H
