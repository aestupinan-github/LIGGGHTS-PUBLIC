/* ----------------------------------------------------------------------
   Surrogate Interface Validator
   Wraps low-level interface functions with LAMMPS error handling
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#ifndef LMP_SURROGATE_VALIDATOR_H
#define LMP_SURROGATE_VALIDATOR_H

#include "surrogate_interface.h"
#include "error.h"
#include <cstdio>

namespace LAMMPS_NS {

/**
 * @brief Interface validation class
 * 
 * Wraps SurrogateInterface functions with LAMMPS error handling.
 * Performs all input validation and output range checks required by Spec 00 & 04.
 * 
 * Usage:
 *   SurrogateValidator validator(lmp->error);
 *   validator.validate_characteristic_length(L_actual, particle_id);
 *   validator.validate_z_unit(z_unit, particle_id);
 *   ...
 */
class SurrogateValidator {
public:
  Error *error;
  
  explicit SurrogateValidator(Error *error_ptr) : error(error_ptr) {}
  
  /* ====================================================================
     Characteristic Length Validation (Spec 00, R00.5)
  ==================================================================== */
  
  /**
   * @brief Validate characteristic length
   * 
   * Constraint: L_actual > 0 and finite
   * Error if invalid; proceed silently if valid
   * 
   * Spec 00, R00.5
   * 
   * @param L_actual    Characteristic length (e.g., cube edge)
   * @param particle_id Particle ID (for error message)
   * 
   * @throws FLERR if L_actual <= 0 or not finite
   */
  void validate_characteristic_length(double L_actual, int particle_id)
  {
    if (!SurrogateInterface::is_characteristic_length_valid(L_actual)) {
      char msg[512];
      snprintf(msg, 512,
        "Invalid characteristic_length at particle %d: L_actual=%g (must be > 0 and finite)",
        particle_id, L_actual);
      error->one(FLERR, msg);
    }
  }
  
  /* ====================================================================
     Z-Unit Validation (Spec 00, R00.1; Spec 04, R04.3)
  ==================================================================== */
  
  /**
   * @brief Validate z_unit (cube centre height above wall / L)
   *
   * Errors on NaN/Inf. Range handling (prefilter / too-deep warning) is done
   * by the caller using z_prefilter and sd_too_deep from scalers.dat.
   *
   * @param z_unit      Normalized centre height
   * @param particle_id Particle ID (for messages)
   *
   * @throws FLERR if z_unit is NaN or Inf
   */
  void validate_z_unit(double z_unit, int particle_id)
  {
    if (!SurrogateInterface::is_finite(z_unit)) {
      char msg[512];
      snprintf(msg, 512,
        "Invalid z_unit (NaN/Inf) at particle %d", particle_id);
      error->one(FLERR, msg);
    }
  }
  
  /* ====================================================================
     Quaternion Validation (Spec 00, R00.1, R00.6; Spec 04, R04.3)
  ==================================================================== */
  
  /**
   * @brief Validate quaternion before canonicalization
   * 
   * Check for NaN/Inf; optionally normalize if not unit magnitude
   * 
   * Spec 00, R00.1 & R00.6 & R04.3
   * 
   * @param quat        Quaternion [qw, qx, qy, qz] (4 elements)
   * @param particle_id Particle ID (for error message)
   * 
   * @throws FLERR if quaternion contains NaN or Inf
   */
  void validate_quaternion(const double quat[4], int particle_id)
  {
    if (!SurrogateInterface::is_quaternion_valid(quat)) {
      char msg[512];
      snprintf(msg, 512,
        "Invalid quaternion (contains NaN/Inf) at particle %d: [%g, %g, %g, %g]",
        particle_id, quat[0], quat[1], quat[2], quat[3]);
      error->one(FLERR, msg);
    }
  }
  
  /**
   * @brief Canonicalize and validate quaternion
   * 
   * Steps:
   * 1. Validate (error on NaN/Inf)
   * 2. Canonicalize (flip to qw >= 0)
   * 3. Normalize (to unit magnitude, defensive)
   * 
   * @param quat        Quaternion [qw, qx, qy, qz] (modified in-place)
   * @param particle_id Particle ID (for error messages)
   * 
   * @throws FLERR if invalid
   */
  void canonicalize_quaternion(double quat[4], int particle_id)
  {
    validate_quaternion(quat, particle_id);
    
    // Flip to canonical form (qw >= 0)
    SurrogateInterface::canonicalize_quaternion(quat);
    
    // Defensive normalization
    double mag = SurrogateInterface::normalize_quaternion(quat);
    if (mag < 1e-15) {
      char msg[512];
      snprintf(msg, 512,
        "Degenerate quaternion (magnitude ≈ 0) at particle %d: [%g, %g, %g, %g]",
        particle_id, quat[0], quat[1], quat[2], quat[3]);
      error->one(FLERR, msg);
    }
  }
  
  /* ====================================================================
     Model Output Validation (Spec 04, R04.3)
  ==================================================================== */
  
  /**
   * @brief Validate raw model output (scaled residual)
   *
   * @param raw_output    Raw network output
   * @param z_unit        Input z_unit (for diagnostic)
   * @param particle_id   Particle ID (for messages)
   *
   * @throws FLERR if raw_output is NaN or Inf
   */
  void validate_model_output(double raw_output, double z_unit, int particle_id)
  {
    if (!SurrogateInterface::is_finite(raw_output)) {
      char msg[512];
      snprintf(msg, 512,
        "Surrogate inference produced NaN/Inf at particle %d with z_unit=%g",
        particle_id, z_unit);
      error->one(FLERR, msg);
    }
  }
  
  /**
   * @brief Validate denormalized signed distance (unit cube)
   * 
   * Check for NaN/Inf after denormalization
   * 
   * @param overlap_unit  Denormalized signed distance sd_unit
   * @param particle_id   Particle ID (for error message)
   * 
   * @throws FLERR if overlap_unit is NaN or Inf
   */
  void validate_denormalized_overlap(double overlap_unit, int particle_id)
  {
    if (!SurrogateInterface::is_finite(overlap_unit)) {
      char msg[512];
      snprintf(msg, 512,
        "Overflow/underflow in denormalization at particle %d: overlap_unit=%g",
        particle_id, overlap_unit);
      error->one(FLERR, msg);
    }
  }
  
  /**
   * @brief Validate physical overlap
   * 
   * Check for NaN/Inf after physical rescaling
   * 
   * @param overlap_phys  Physical overlap
   * @param particle_id   Particle ID (for error message)
   * 
   * @throws FLERR if overlap_phys is NaN or Inf
   */
  void validate_physical_overlap(double overlap_phys, int particle_id)
  {
    if (!SurrogateInterface::is_finite(overlap_phys)) {
      char msg[512];
      snprintf(msg, 512,
        "Invalid physical overlap at particle %d: overlap_phys=%g",
        particle_id, overlap_phys);
      error->one(FLERR, msg);
    }
  }
  
  /* ====================================================================
     Composite Validation Pipelines
  ==================================================================== */
  
  /**
   * @brief Full input validation pipeline
   * 
   * Validates all inputs before surrogate query:
   * - Characteristic length (L_actual > 0)
   * - Z-unit (finite, in extrapolation threshold)
   * - Quaternion (finite, canonical, unit norm)
   * 
   * Spec 04, R04.3 (Before Surrogate Query)
   * 
   * @param z_phys      Physical height above wall
   * @param L_actual    Characteristic length
   * @param quat_input  Input quaternion [qw, qx, qy, qz]
   * @param particle_id Particle ID (for error messages)
   * 
   * @throws FLERR if any validation fails
   */
  void validate_inputs_for_surrogate(
    double z_phys,
    double L_actual,
    const double quat_input[4],
    int particle_id)
  {
    validate_characteristic_length(L_actual, particle_id);
    
    double z_unit = z_phys / L_actual;
    validate_z_unit(z_unit, particle_id);
    
    validate_quaternion(quat_input, particle_id);
    // Note: Don't canonicalize here; just validate
    // Canonicalization happens in prepare_input_tensor
  }
  
  /**
   * @brief Full output pipeline: raw output -> physical signed distance
   *
   * sd_unit = (raw*y_scale + y_mean) - h_unit ;  sd_phys = sd_unit * L_actual
   * (run_029 option4 residual model; see SurrogateInterface::denormalize_signed_distance)
   *
   * @param raw_output    Raw network output (scaled residual)
   * @param z_unit        Input z_unit (for diagnostic)
   * @param y_mean        Scaler mean
   * @param y_scale       Scaler scale
   * @param h_unit        Exact support height h(q) for the unit cube
   * @param L_actual      Characteristic length
   * @param particle_id   Particle ID (for error messages)
   * @param sd_unit_out   Output: dimensionless signed distance
   *
   * @return sd_phys      Physical signed distance (>0 gap, <0 penetration)
   *
   * @throws FLERR if any value is NaN/Inf
   */
  double validate_outputs_from_surrogate(
    double raw_output,
    double z_unit,
    double y_mean,
    double y_scale,
    double h_unit,
    double L_actual,
    int particle_id,
    double &sd_unit_out)
  {
    validate_model_output(raw_output, z_unit, particle_id);
    
    double sd_unit = SurrogateInterface::denormalize_signed_distance(
      raw_output, y_mean, y_scale, h_unit);
    validate_denormalized_overlap(sd_unit, particle_id);
    
    double sd_phys = SurrogateInterface::rescale_to_physical(sd_unit, L_actual);
    validate_physical_overlap(sd_phys, particle_id);
    
    sd_unit_out = sd_unit;
    return sd_phys;
  }
};

} // namespace LAMMPS_NS

#endif // LMP_SURROGATE_VALIDATOR_H
