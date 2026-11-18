/* ----------------------------------------------------------------------
   Surrogate Contact Model Manager
   Handles model loading, inference, and denormalization for surrogate particles
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#ifndef LMP_SURROGATE_CONTACT_H
#define LMP_SURROGATE_CONTACT_H

#include <memory>
#include <vector>
#include <torch/torch.h>
#include <torch/script.h>

#include "surrogate_interface.h"
#include "surrogate_validator.h"
#include "surrogate_path_resolver.h"
#include "scaler_parser.h"
#include "error.h"

namespace LAMMPS_NS {

/**
 * @brief Manages surrogate model loading and inference for contact calculations
 * 
 * Responsibilities:
 * - Load LibTorch models and scalers from disk
 * - Validate model metadata (shape type, training parameters)
 * - Execute model inference with validation
 * - Denormalize outputs to physical units using generalized normal distance
 * - Report diagnostic information
 * 
 * Thread-safe model inference (read-only after initialization)
 * 
 * **Model contract (run_029, option4 residual; verified against training code):**
 * - Input:  z_unit = (cube centre distance to wall) / L_actual, quaternion [qw,qx,qy,qz]
 *           (float32, the network weights are float32). Training range z_unit in [0.40, 0.966].
 * - Output: scaled residual r_s;  sd_unit = (r_s*y_scale + y_mean) - h_exact(q)
 * - sd > 0 gap, sd < 0 penetration (same sign as LIGGGHTS deltan)
 * - Shape: inferred from folder path /SS_models/{shape_name}-walls/
 */
class SurrogateContactModel {
public:
  Error *error;
  
  // Model and scaler data
  torch::jit::script::Module model;                       // Loaded LibTorch model
  ScalerRecord scaler_record;                             // y_mean, y_scale, run_id, etc.
  std::string loaded_shape_name;                          // Shape this model is trained for
  std::string model_file_path;                            // For diagnostics
  std::string scaler_file_path;                           // For diagnostics
  bool model_loaded_;                                     // Flag: model successfully loaded?
  
  SurrogateValidator validator;
  
  explicit SurrogateContactModel(Error *error_ptr)
    : error(error_ptr), model_loaded_(false), validator(error_ptr) {}
  
  ~SurrogateContactModel() = default;
  
  /**
   * @brief Load surrogate model and scalers for a shape
   * 
   * Steps:
   * 1. Resolve model path using SurrogatePathResolver
   * 2. Load LibTorch model (.pt file) using torch::jit::load
   * 3. Load and parse scalers (.dat file)
   * 4. Validate model metadata (input dimension must be 5)
   * 5. Store shape name and model info
   * 
   * Spec 04, R04.5 (Model Identity Validation)
   * 
   * @param shape_name Shape identifier (e.g., "ss-cube")
   * 
   * @throws FLERR if model not found, load fails, or metadata invalid
   */
  void load_model(const char *shape_name)
  {
#ifdef SURROGATE_ACTIVE_FLAG
    if (!shape_name || strlen(shape_name) == 0) {
      error->one(FLERR, "Surrogate model: shape_name is empty");
    }
    
    loaded_shape_name = std::string(shape_name);
    
    try {
      // Step 1: Resolve model path
      SurrogatePathResolver path_resolver;
      model_file_path = path_resolver.resolve_model_path(shape_name);
      scaler_file_path = path_resolver.resolve_scaler_path(shape_name);
      
      // Step 2: Load LibTorch model
      // torch::jit::load returns a Module object (not shared_ptr)
      model = torch::jit::load(model_file_path);
      
      // Step 3: Load and parse scalers
      std::string parse_error;
      if (!ScalerParser::parse_file(scaler_file_path, scaler_record, parse_error)) {
        char msg[1024];
        snprintf(msg, 1024,
          "Failed to parse scalers for shape '%s':\n  File: %s\n  Error: %s",
          shape_name, scaler_file_path.c_str(), parse_error.c_str());
        error->one(FLERR, msg);
      }
      
      // Step 4: Validate metadata
      if (scaler_record.network_input_dim != 5) {
        char msg[512];
        snprintf(msg, 512,
          "Model input dimension mismatch: expected 5, got %d",
          scaler_record.network_input_dim);
        error->one(FLERR, msg);
      }
      
      // Only the option4 residual contract (sd = residual - h_exact) is implemented
      if (scaler_record.model_type != "option4") {
        char msg[512];
        snprintf(msg, 512,
          "Surrogate model_type '%s' not supported (only 'option4': sd = out*y_scale + y_mean - h_exact)",
          scaler_record.model_type.c_str());
        error->one(FLERR, msg);
      }
      
      model_loaded_ = true;
    }
    catch (const std::exception &e) {
      char msg[1024];
      snprintf(msg, 1024,
        "Failed to load surrogate model for shape '%s':\n  Model: %s\n  Scalers: %s\n  Error: %s",
        shape_name, model_file_path.c_str(), scaler_file_path.c_str(), e.what());
      error->one(FLERR, msg);
    }
#endif
  }
  
  /**
   * @brief Query surrogate model for the signed distance between cube and wall
   *
   * Pipeline (run_029 option4 residual model):
   * 1. Validate inputs (z_unit finite, L_actual > 0, quaternion finite)
   * 2. Canonicalize quaternion (qw >= 0), pack [z_unit, qw, qx, qy, qz]
   * 3. raw = model.forward(position_z[1], quat[1,4])   (float32)
   * 4. h   = h_exact_cube(q, dir_wall)
   * 5. sd_unit = (raw*y_scale + y_mean) - h ;  sd_phys = sd_unit * L_actual
   *
   * The caller is responsible for the z_prefilter skip (no contact possible).
   *
   * @param z_unit       Cube centre distance to wall divided by L_actual
   * @param L_actual     Particle characteristic length (cube edge)
   * @param quat_input   Particle orientation [qw, qx, qy, qz]
   * @param dir_wall     Unit vector from particle centre toward the wall (world frame)
   * @param particle_id  Particle ID (for error messages)
   * @param step         Simulation step (for error context)
   * @param sd_unit_out  Output: dimensionless signed distance (for diagnostics/range checks)
   *
   * @return sd_phys     Physical signed distance; > 0 gap, <= 0 contact (penetration depth = -sd_phys)
   *
   * @throws FLERR if any validation fails or model inference errors
   */
  double query_overlap(
    double z_unit,
    double L_actual,
    const double quat_input[4],
    const double dir_wall[3],
    int particle_id,
    int step,
    double &sd_unit_out)
  {
#ifdef SURROGATE_ACTIVE_FLAG
    // Step 1: Validate inputs
    validator.validate_inputs_for_surrogate(z_unit * L_actual, L_actual, quat_input, particle_id);
    
    // Step 2: Prepare input [z_unit, qw, qx, qy, qz] with canonical quaternion
    double input_tensor[5];
    double quat_copy[4] = {quat_input[0], quat_input[1], quat_input[2], quat_input[3]};
    SurrogateInterface::prepare_input_tensor(z_unit * L_actual, L_actual, quat_copy, input_tensor);
    double quat_canon[4] = {input_tensor[1], input_tensor[2], input_tensor[3], input_tensor[4]};
    SurrogateInterface::normalize_quaternion(quat_canon);
    
    try {
      // Step 3: Model forward. Signature forward(position_z [B], quat [B,4]); weights float32.
      torch::Tensor position_z = torch::tensor({(float)input_tensor[0]}, torch::kFloat32);
      torch::Tensor quat_tensor = torch::tensor(
        {(float)quat_canon[0], (float)quat_canon[1], (float)quat_canon[2], (float)quat_canon[3]},
        torch::kFloat32).reshape({1, 4});
      
      std::vector<torch::jit::IValue> model_input;
      model_input.push_back(position_z);
      model_input.push_back(quat_tensor);
      torch::Tensor output;
      {
        torch::NoGradGuard no_grad;
        output = model.forward(model_input).toTensor();
      }
      double raw_output = output.reshape({-1})[0].item<double>();
      
      // Step 4: exact support height toward the wall
      double h_unit = SurrogateInterface::h_exact_cube(quat_canon, dir_wall);
      
      // Step 5: denormalize + rescale
      return validator.validate_outputs_from_surrogate(
        raw_output, z_unit,
        scaler_record.y_mean, scaler_record.y_scale,
        h_unit, L_actual, particle_id, sd_unit_out);
    }
    catch (const std::exception &e) {
      char msg[1024];
      snprintf(msg, 1024,
        "Surrogate model inference failed at particle %d, step %d, shape '%s', z_unit=%.6e:\n  Error: %s",
        particle_id, step, loaded_shape_name.c_str(), z_unit, e.what());
      error->one(FLERR, msg);
    }
    
    return 0.0;  // Unreachable (error->one exits)
#else
    error->one(FLERR, "Surrogate contact requires ENABLE_SURROGATE=ON at compile time");
    return 0.0;
#endif
  }
  
  /**
   * @brief Batched query: signed distances for many particles in ONE model forward pass
   *
   * Same contract as query_overlap() (Spec 00 R00.1-R00.5), evaluated for n inputs at once
   * (Spec 04 R04.2f). Avoids the ~11-15 us fixed LibTorch overhead per call; per particle the
   * cost drops to ~0.4 us at batch >= 100 (micro-benchmark 2026-11-18).
   *
   * @param n           number of queries
   * @param z_unit      [n] centre distance to wall / L
   * @param L_actual    [n] characteristic length
   * @param quat        [n][4] orientations [qw, qx, qy, qz] (any sign, any norm)
   * @param dir_wall    [n][3] unit vectors centre -> wall
   * @param particle_id [n] particle IDs (error messages)
   * @param step        time step (error messages)
   * @param sd_unit_out [n] output: dimensionless signed distance
   * @param sd_phys_out [n] output: physical signed distance (> 0 gap, <= 0 contact)
   */
  void query_overlap_batch(int n, const double *z_unit, const double *L_actual,
                           const double (*quat)[4], const double (*dir_wall)[3],
                           const int *particle_id, int step,
                           double *sd_unit_out, double *sd_phys_out)
  {
#ifdef SURROGATE_ACTIVE_FLAG
    if (n <= 0) return;
    std::vector<float> zf(n), qf(4 * (size_t)n);
    std::vector<double> qc(4 * (size_t)n);
    for (int i = 0; i < n; i++) {
      validator.validate_inputs_for_surrogate(z_unit[i] * L_actual[i], L_actual[i], quat[i], particle_id[i]);
      double qq[4] = {quat[i][0], quat[i][1], quat[i][2], quat[i][3]};
      SurrogateInterface::canonicalize_quaternion(qq);
      SurrogateInterface::normalize_quaternion(qq);
      zf[i] = (float)z_unit[i];
      for (int k = 0; k < 4; k++) { qf[4*i+k] = (float)qq[k]; qc[4*i+k] = qq[k]; }
    }
    try {
      torch::Tensor pz = torch::from_blob(zf.data(), {n}, torch::kFloat32);
      torch::Tensor qt = torch::from_blob(qf.data(), {n, 4}, torch::kFloat32);
      std::vector<torch::jit::IValue> in{pz, qt};
      torch::Tensor out;
      {
        torch::NoGradGuard no_grad;
        out = model.forward(in).toTensor().reshape({-1}).to(torch::kFloat64).contiguous();
      }
      const double *raw = out.data_ptr<double>();
      for (int i = 0; i < n; i++) {
        const double h = SurrogateInterface::h_exact_cube(&qc[4*i], dir_wall[i]);
        sd_phys_out[i] = validator.validate_outputs_from_surrogate(
          raw[i], z_unit[i], scaler_record.y_mean, scaler_record.y_scale,
          h, L_actual[i], particle_id[i], sd_unit_out[i]);
      }
    }
    catch (const std::exception &e) {
      char msg[1024];
      snprintf(msg, 1024,
        "Surrogate batched inference failed (%d queries) at step %d, shape '%s':\n  Error: %s",
        n, step, loaded_shape_name.c_str(), e.what());
      error->one(FLERR, msg);
    }
#else
    error->one(FLERR, "Surrogate contact requires ENABLE_SURROGATE=ON at compile time");
#endif
  }

  /**
   * @brief Check if model is loaded and ready
   * 
   * @return true if model successfully loaded; false otherwise
   */
  bool is_loaded() const
  {
    return model_loaded_;
  }
  
  /**
   * @brief Get shape name this model is trained for
   * 
   * **Phase 3C Assumption:** Shape is inferred from folder path
   * /SS_models/{shape_name}-walls/model.pt
   * 
   * @return Shape name string
   */
  const std::string& get_shape_name() const
  {
    return loaded_shape_name;
  }
};

} // namespace LAMMPS_NS

#endif // LMP_SURROGATE_CONTACT_H
