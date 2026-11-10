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

/// Epsilon for preventing log domain underflow in denormalization
/// When overlap_unit = 10^(output) - eps, this prevents underflow
static constexpr double EPS_DENORM = 1e-15;

/// Training range for z_unit (normalized height above wall)
/// Outside this range is extrapolation (permissible but may warn)
static constexpr double Z_UNIT_MIN_TRAIN = -0.5;
static constexpr double Z_UNIT_MAX_TRAIN = 2.0;

/// Extrapolation warning threshold (multiples of training range)
static constexpr double Z_UNIT_EXTRAP_THRESHOLD = 10.0;

/// Output range for log10(overlap_unit) (approximately)
/// Outside this range is suspicious (may warn)
static constexpr double LOG10_OVERLAP_MIN_TRAIN = -5.0;
static constexpr double LOG10_OVERLAP_MAX_TRAIN = 0.5;

/* =====================================================================
   Denormalization Functions (Spec 00, R00.3-R00.4)
===================================================================== */

/**
 * @brief Denormalize model output from log10 scale to physical overlap
 * 
 * Formula: overlap_unit = 10^(log10_output * y_scale + y_mean) - eps
 * For current models (y_mean=0, y_scale=1): overlap_unit = 10^(log10_output) - eps
 * 
 * Spec 00, R00.3-R00.4
 * 
 * @param log10_output Raw model output (log10 scale, float)
 * @param y_mean       Mean from scalers (normalization parameter)
 * @param y_scale      Scale from scalers (normalization parameter)
 * @param eps          Epsilon to prevent underflow (typically 1e-15)
 * 
 * @return overlap_unit Denormalized overlap (nondimensional)
 * 
 * @note For current models (run_029, run_030): y_mean=0, y_scale=1
 *       so formula simplifies to: 10^(log10_output) - eps
 */
inline double denormalize_overlap(double log10_output, double y_mean, double y_scale, double eps)
{
  // Reverse normalization: exponentiate + remove eps
  double overlap_unit = std::pow(10.0, (log10_output * y_scale + y_mean)) - eps;
  return overlap_unit;
}

/**
 * @brief Denormalize overlap from nondimensional to physical units
 * 
 * Formula: overlap_phys = overlap_unit * L_actual
 * 
 * Spec 00, R00.4-R00.5
 * 
 * @param overlap_unit Denormalized overlap (nondimensional)
 * @param L_actual     Particle characteristic length (e.g., cube edge)
 * 
 * @return overlap_phys Physical overlap in simulation units
 * 
 * @note Always check L_actual > 0 before calling
 */
inline double rescale_overlap_physical(double overlap_unit, double L_actual)
{
  return overlap_unit * L_actual;
}

/**
 * @brief Combined denormalization: log10 output → physical overlap
 * 
 * Convenience function combining denormalization + physical rescaling
 * 
 * @param log10_output Raw model output
 * @param y_mean       Normalization mean
 * @param y_scale      Normalization scale
 * @param L_actual     Characteristic length
 * @param eps          Underflow epsilon
 * 
 * @return overlap_phys Physical overlap
 */
inline double denormalize_overlap_full(
  double log10_output, 
  double y_mean, 
  double y_scale, 
  double L_actual, 
  double eps = EPS_DENORM)
{
  double overlap_unit = denormalize_overlap(log10_output, y_mean, y_scale, eps);
  return rescale_overlap_physical(overlap_unit, L_actual);
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
   Input Validation (Spec 00, R04.3)
===================================================================== */

/**
 * @brief Check if z_unit is within extrapolation threshold
 * 
 * Training range: [-0.5, +2.0]
 * Extrapolation threshold: ±10.0
 * 
 * @param z_unit Normalized height above wall
 * 
 * @return true if within threshold; false if extrapolation is severe
 * 
 * @note If false, system should warn but proceed (extrapolation is permissible)
 */
inline bool is_z_unit_in_extrapolation_threshold(double z_unit)
{
  return (z_unit >= -Z_UNIT_EXTRAP_THRESHOLD && z_unit <= Z_UNIT_EXTRAP_THRESHOLD);
}

/**
 * @brief Check if z_unit is within training range
 * 
 * Training range: [-0.5, +2.0]
 * 
 * @param z_unit Normalized height above wall
 * 
 * @return true if within training range; false if extrapolating
 */
inline bool is_z_unit_in_training_range(double z_unit)
{
  return (z_unit >= Z_UNIT_MIN_TRAIN && z_unit <= Z_UNIT_MAX_TRAIN);
}

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

/**
 * @brief Check if model output is within expected training range
 * 
 * Training output range: approximately [-5.0, +0.5] (log10 scale)
 * Outside this range is permissible but suspicious (may warn)
 * 
 * @param log10_output Model output (log10 scale)
 * 
 * @return true if within training range; false if suspicious
 */
inline bool is_model_output_in_training_range(double log10_output)
{
  return (log10_output >= LOG10_OVERLAP_MIN_TRAIN && log10_output <= LOG10_OVERLAP_MAX_TRAIN);
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
