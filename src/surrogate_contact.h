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
 * **Phase 3C Design Notes:**
 * - Normal direction: Uses `delta` from primitiveWall_->resolveContact()
 *   Computes s_unit = |deltan| / L_actual (works for any wall orientation)
 * - Shape mismatch: Inferred from folder path /SS_models/{shape_name}-walls/
 *   (Phase 3D+: will add explicit metadata field to scalers.dat)
 * - Extrapolation: Skips if s_unit > 3 * L_actual (to be reviewed for elongated shapes)
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
   * @brief Query surrogate model for contact overlap
   * 
   * **Phase 3C: Generalized Normal Distance**
   * - Input: s_unit (normalized distance along wall normal), quaternion
   * - s_unit is computed as: |deltan| / L_actual
   * - deltan comes from primitiveWall_->resolveContact()
   * - This works for walls in ANY orientation (xy, yz, xz planes, cylinders, etc.)
   * 
   * Steps (Spec 04, R04.2 Contact Calculation Flow):
   * 1. Validate inputs (s_unit, L_actual, quaternion)
   * 2. Prepare input tensor [s_unit, qw, qx, qy, qz]
   * 3. Query model: log10_output = model.forward(input_tensor)
   * 4. Validate model output
   * 5. Denormalize: overlap_unit = 10^(log10_output) - eps
   * 6. Rescale: overlap_phys = overlap_unit * L_actual
   * 7. Check physical validity (overlap <= 0 for wall contact)
   * 
   * @param s_unit          Normalized distance along wall normal = |deltan| / L_actual
   * @param L_actual        Particle characteristic length
   * @param quat_input      Particle orientation [qw, qx, qy, qz]
   * @param particle_id     Particle ID (for error messages)
   * @param step            Simulation step (for error context)
   * 
   * @return overlap_phys   Physical overlap
   *                        ≤ 0 for contact, > 0 for gap
   * 
   * @throws FLERR if any validation fails or model inference errors
   */
  double query_overlap(
    double s_unit,
    double L_actual,
    double quat_input[4],
    int particle_id,
    int step)
  {
#ifdef SURROGATE_ACTIVE_FLAG
    // Reconstruct z_phys from s_unit for validator (it uses z_phys internally)
    // Note: z_phys is not used directly; validator uses it only for range checking
    double z_phys = s_unit * L_actual;
    
    // Step 1: Validate inputs
    validator.validate_inputs_for_surrogate(z_phys, L_actual, quat_input, particle_id);
    
    // Step 2: Prepare input tensor
    double input_tensor[5];
    SurrogateInterface::prepare_input_tensor(z_phys, L_actual, quat_input, input_tensor);
    
    double z_unit = input_tensor[0];  // This is actually s_unit (generalized normal distance)
    
    try {
      // Step 3: Query model
      // Model expects: position_z as scalar/[1] and quat as [1, 4]
      // Model internally does: torch.unsqueeze(position_z, 1) -> [1, 1]
      //                        torch.cat([unsqueezed_position_z, quat_norm], 1) -> [1, 5]
      double pos_z = input_tensor[0];  // z_unit
      double quat_data[4] = {input_tensor[1], input_tensor[2], input_tensor[3], input_tensor[4]};
      
      // Convert to torch tensors
      // position_z should be shape [1] (batch size 1 with scalar value)
      torch::Tensor position_z = torch::from_blob(&pos_z, {1}, torch::kFloat64);
      torch::Tensor quat_tensor = torch::from_blob(quat_data, {1, 4}, torch::kFloat64);
      
      // Get device and dtype from model parameters
      auto params = model.parameters();
      auto param_iter = params.begin();
      torch::Device device = (*param_iter).device();
      torch::ScalarType dtype = (*param_iter).scalar_type();
      
      position_z = position_z.to(device).to(dtype);
      quat_tensor = quat_tensor.to(device).to(dtype);
      
      // Forward pass with two arguments (wrapped in IValue for torch::jit::Module::forward)
      std::vector<torch::jit::IValue> model_input;
      model_input.push_back(position_z);
      model_input.push_back(quat_tensor);
      torch::Tensor output = model.forward(model_input).toTensor();
      double log10_output = output[0].item<double>();
      
      // Step 4: Validate model output
      validator.validate_model_output(log10_output, z_unit, particle_id);
      
      // Step 5-6: Denormalize and rescale
      double overlap_phys = validator.validate_outputs_from_surrogate(
        log10_output, z_unit,
        scaler_record.y_mean, scaler_record.y_scale,
        L_actual,
        particle_id);
      
      // Step 7: Check physical validity (wall contact: overlap ≤ 0 means contact)
      if (overlap_phys > 0.0) {
        // No contact; gap exists
        return overlap_phys;  // Return positive (will be used to skip force calc)
      }
      
      return overlap_phys;  // Return negative or zero (contact)
      
    }
    catch (const std::exception &e) {
      char msg[1024];
      snprintf(msg, 1024,
        "Surrogate model inference failed at particle %d, step %d, shape '%s', s_unit=%.6e:\n  Error: %s",
        particle_id, step, loaded_shape_name.c_str(), s_unit, e.what());
      error->one(FLERR, msg);
    }
    
    return 0.0;  // Unreachable (error->one exits)
#else
    error->one(FLERR, "Surrogate contact requires ENABLE_SURROGATE=ON at compile time");
    return 0.0;
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
