/* ----------------------------------------------------------------------
   Model path resolution for Surrogate Shape models
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#ifndef LMP_SURROGATE_PATH_RESOLVER_H
#define LMP_SURROGATE_PATH_RESOLVER_H

#include <string>
#include <vector>

namespace LAMMPS_NS {

/**
 * @brief Resolves paths to surrogate models and scalers files
 * 
 * Implements 4-step fallback algorithm per Spec 02 R02.3:
 * 1. Environment variable SS_MODELS_DIR
 * 2. Relative to executable: ./SS_models/
 * 3. Relative to LIGGGHTS source: ../SS_models/
 * 4. System install: /usr/local/liggghts/SS_models/
 */
class SurrogatePathResolver {
 public:
  /**
   * @brief Resolve the root SS_models directory
   * 
   * @param error_callback Function to call on error (receives diagnostic message)
   * @return std::string Path to SS_models directory, or empty string if not found
   */
  static std::string resolve_models_dir(void (*error_callback)(const std::string&) = nullptr);

  /**
   * @brief Resolve full path to model.pt for a given shape
   * 
   * @param shape_name Shape name (e.g., "ss-cube")
   * @param error_callback Function to call on error
   * @return std::string Full path to model.pt, or empty string if not found
   */
  static std::string resolve_model_path(const std::string& shape_name,
                                        void (*error_callback)(const std::string&) = nullptr);

  /**
   * @brief Resolve full path to scalers.dat for a given shape
   * 
   * @param shape_name Shape name (e.g., "ss-cube")
   * @param error_callback Function to call on error
   * @return std::string Full path to scalers.dat, or empty string if not found
   */
  static std::string resolve_scaler_path(const std::string& shape_name,
                                         void (*error_callback)(const std::string&) = nullptr);

 private:
  /**
   * @brief Check if a file exists
   * 
   * @param filepath Full path to file
   * @return true if file exists and is readable
   */
  static bool file_exists(const std::string& filepath);

  /**
   * @brief Get directory of running executable
   * 
   * @return std::string Directory path, or empty string if not determinable
   */
  static std::string get_exe_dir();

  /**
   * @brief Get LIGGGHTS source directory (best guess)
   * 
   * @return std::string Directory path, or empty string if not determinable
   */
  static std::string get_source_dir();
};

} // namespace LAMMPS_NS

#endif /* LMP_SURROGATE_PATH_RESOLVER_H */
