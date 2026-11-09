/* ----------------------------------------------------------------------
   Model path resolution implementation for Surrogate Shape models
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#include "surrogate_path_resolver.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <libgen.h>

using namespace LAMMPS_NS;

// Static helper to check file existence
bool SurrogatePathResolver::file_exists(const std::string& filepath) {
  std::ifstream f(filepath);
  return f.good();
}

// Get directory of running executable (Linux specific)
std::string SurrogatePathResolver::get_exe_dir() {
  char exe_path[4096] = {0};
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len == -1) return "";
  
  exe_path[len] = '\0';
  std::string path_str(exe_path);
  size_t last_slash = path_str.rfind('/');
  if (last_slash != std::string::npos) {
    return path_str.substr(0, last_slash);
  }
  return "";
}

// Get LIGGGHTS source directory (best guess: parent of build dir)
std::string SurrogatePathResolver::get_source_dir() {
  // Try to get current working directory
  char cwd[4096] = {0};
  if (getcwd(cwd, sizeof(cwd)) == nullptr) return "";
  
  std::string cwd_str(cwd);
  
  // Heuristic: if we're in a 'build' directory, parent is source
  size_t build_pos = cwd_str.rfind("/build");
  if (build_pos != std::string::npos && build_pos == cwd_str.length() - 6) {
    return cwd_str.substr(0, build_pos);
  }
  
  // Otherwise assume cwd is source
  return cwd_str;
}

std::string SurrogatePathResolver::resolve_models_dir(void (*error_callback)(const std::string&)) {
  std::vector<std::string> search_paths;
  std::string sep = "/";
  
  // Step 1: Environment variable
  const char* env_models_dir = std::getenv("SS_MODELS_DIR");
  if (env_models_dir) {
    search_paths.push_back(std::string(env_models_dir));
  }
  
  // Step 2: Relative to executable
  std::string exe_dir = get_exe_dir();
  if (!exe_dir.empty()) {
    search_paths.push_back(exe_dir + sep + "SS_models");
  }
  
  // Step 3: Relative to source
  std::string src_dir = get_source_dir();
  if (!src_dir.empty()) {
    search_paths.push_back(src_dir + sep + "SS_models");
  }
  
  // Step 4: System install
  search_paths.push_back("/usr/local/liggghts/SS_models");
  
  // Search for existing directory
  for (const auto& path : search_paths) {
    std::string check_file = path + "/ss-cube-walls/model.pt";
    // Simple heuristic: check if ss-cube-walls subdir exists
    if (file_exists(check_file)) {
      return path;
    }
  }
  
  // Not found: report diagnostic
  if (error_callback) {
    std::ostringstream oss;
    oss << "Surrogate models directory not found. Searched:\n";
    for (const auto& path : search_paths) {
      oss << "  - " << path << "\n";
    }
    oss << "Set SS_MODELS_DIR environment variable or copy SS_models/ to one of these locations.";
    error_callback(oss.str());
  }
  
  return "";
}

std::string SurrogatePathResolver::resolve_model_path(const std::string& shape_name,
                                                      void (*error_callback)(const std::string&)) {
  std::string models_dir = resolve_models_dir(error_callback);
  if (models_dir.empty()) {
    return "";
  }
  
  std::string model_path = models_dir + "/" + shape_name + "-walls/model.pt";
  
  if (!file_exists(model_path)) {
    if (error_callback) {
      std::ostringstream oss;
      oss << "Model file not found: " << model_path << "\n"
          << "Expected location for shape '" << shape_name << "': "
          << models_dir << "/" << shape_name << "-walls/";
      error_callback(oss.str());
    }
    return "";
  }
  
  return model_path;
}

std::string SurrogatePathResolver::resolve_scaler_path(const std::string& shape_name,
                                                       void (*error_callback)(const std::string&)) {
  std::string models_dir = resolve_models_dir(error_callback);
  if (models_dir.empty()) {
    return "";
  }
  
  std::string scaler_path = models_dir + "/" + shape_name + "-walls/scalers.dat";
  
  if (!file_exists(scaler_path)) {
    if (error_callback) {
      std::ostringstream oss;
      oss << "Scaler file not found: " << scaler_path << "\n"
          << "Expected location for shape '" << shape_name << "': "
          << models_dir << "/" << shape_name << "-walls/";
      error_callback(oss.str());
    }
    return "";
  }
  
  return scaler_path;
}
