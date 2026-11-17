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
  const double w = quat[0], x = quat[1], y = quat[2], z = quat[3];
  const double R[3][3] = {
    {1 - 2*(y*y + z*z), 2*(x*y - z*w),     2*(x*z + y*w)},
    {2*(x*y + z*w),     1 - 2*(x*x + z*z), 2*(y*z - x*w)},
    {2*(x*z - y*w),     2*(y*z + x*w),     1 - 2*(x*x + y*y)}
  };
  // body-frame direction toward wall: u = R^T d
  double u[3];
  for (int k = 0; k < 3; k++)
    u[k] = R[0][k]*dir_wall[0] + R[1][k]*dir_wall[1] + R[2][k]*dir_wall[2];
  // max over cube vertices (+-a,+-a,+-a) of v.u = a*(|u0|+|u1|+|u2|)
  return CUBE_HALF_EDGE_UNIT * (std::fabs(u[0]) + std::fabs(u[1]) + std::fabs(u[2]));
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
