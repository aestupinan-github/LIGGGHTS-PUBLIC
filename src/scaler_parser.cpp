/* ----------------------------------------------------------------------
   Implementation of ss_model_record_v1 scaler parser
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#include "scaler_parser.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>

using namespace LAMMPS_NS;

bool ScalerParser::string_to_double(const std::string& str, double& out_value) {
  try {
    size_t pos;
    out_value = std::stod(str, &pos);
    return pos == str.length();  // Entire string must be consumed
  } catch (...) {
    return false;
  }
}

bool ScalerParser::string_to_int(const std::string& str, int& out_value) {
  try {
    size_t pos;
    out_value = std::stoi(str, &pos);
    return pos == str.length();  // Entire string must be consumed
  } catch (...) {
    return false;
  }
}

bool ScalerParser::parse_line(const std::string& line,
                             std::string& key,
                             std::string& value) {
  // Trim leading whitespace
  size_t start = 0;
  while (start < line.length() && std::isspace(line[start])) {
    start++;
  }
  
  // Skip empty lines or comments
  if (start >= line.length() || line[start] == '#') {
    return false;
  }
  
  // Find first space (separator between key and value)
  size_t sep = start;
  while (sep < line.length() && !std::isspace(line[sep])) {
    sep++;
  }
  
  if (sep >= line.length()) {
    return false;  // No value found
  }
  
  key = line.substr(start, sep - start);
  
  // Skip spaces after key
  size_t val_start = sep;
  while (val_start < line.length() && std::isspace(line[val_start])) {
    val_start++;
  }
  
  if (val_start >= line.length()) {
    return false;  // No value
  }
  
  // Find end of value (trimming trailing whitespace)
  size_t val_end = line.length() - 1;
  while (val_end > val_start && std::isspace(line[val_end])) {
    val_end--;
  }
  
  value = line.substr(val_start, val_end - val_start + 1);
  return true;
}

bool ScalerParser::parse_file(const std::string& filepath,
                             ScalerRecord& out_record,
                             std::string& error_message) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    error_message = std::string("Cannot open scalers file: ") + filepath;
    return false;
  }
  
  // Initialize defaults
  out_record.network_input_dim = 0;
  out_record.n_ref = 0;
  out_record.y_mean = 0.0;
  out_record.y_scale = 1.0;
  
  std::string line;
  int line_num = 0;
  bool found_format = false;
  
  while (std::getline(file, line)) {
    line_num++;
    std::string key, value;
    
    if (!parse_line(line, key, value)) {
      continue;  // Skip empty/comment lines
    }
    
    // Parse mandatory fields
    if (key == "format") {
      out_record.format = value;
      found_format = true;
    }
    else if (key == "run_id") {
      out_record.run_id = value;
    }
    else if (key == "model_type") {
      out_record.model_type = value;
      if (value != "option4" && value != "option1") {
        error_message = std::string("Invalid model_type '") + value + 
                        "' (must be 'option4' or 'option1')";
        return false;
      }
    }
    else if (key == "input_dtype") {
      out_record.input_dtype = value;
    }
    else if (key == "input_units") {
      out_record.input_units = value;
    }
    else if (key == "y_mean") {
      if (!string_to_double(value, out_record.y_mean)) {
        error_message = std::string("Invalid y_mean value: ") + value;
        return false;
      }
    }
    else if (key == "y_scale") {
      if (!string_to_double(value, out_record.y_scale)) {
        error_message = std::string("Invalid y_scale value: ") + value;
        return false;
      }
    }
    else if (key == "r_circ") {
      if (!string_to_double(value, out_record.r_circ)) {
        error_message = std::string("Invalid r_circ value: ") + value;
        return false;
      }
    }
    else if (key == "z_prefilter") {
      if (!string_to_double(value, out_record.z_prefilter)) {
        error_message = std::string("Invalid z_prefilter value: ") + value;
        return false;
      }
    }
    else if (key == "sd_too_deep") {
      if (!string_to_double(value, out_record.sd_too_deep)) {
        error_message = std::string("Invalid sd_too_deep value: ") + value;
        return false;
      }
    }
    else if (key == "h_exact_method") {
      out_record.h_exact_method = value;
    }
    else if (key == "network_input_dim") {
      if (!string_to_int(value, out_record.network_input_dim)) {
        error_message = std::string("Invalid network_input_dim value: ") + value;
        return false;
      }
    }
    else if (key == "ref_eval") {
      out_record.ref_eval = value;
    }
    else if (key == "ref_sd_tol") {
      if (!string_to_double(value, out_record.ref_sd_tol)) {
        error_message = std::string("Invalid ref_sd_tol value: ") + value;
        return false;
      }
    }
    else if (key == "n_ref") {
      if (!string_to_int(value, out_record.n_ref)) {
        error_message = std::string("Invalid n_ref value: ") + value;
        return false;
      }
    }
    else {
      // Optional field: store for forward compatibility
      out_record.optional_fields[key] = value;
    }
  }
  
  file.close();
  
  // Validate mandatory fields
  if (!found_format) {
    error_message = "Missing mandatory field: format";
    return false;
  }
  
  if (out_record.format != "ss_model_record_v1") {
    error_message = std::string("Unknown format version: ") + out_record.format +
                    " (expected 'ss_model_record_v1')";
    return false;
  }
  
  if (out_record.run_id.empty()) {
    error_message = "Missing mandatory field: run_id";
    return false;
  }
  
  if (out_record.model_type.empty()) {
    error_message = "Missing mandatory field: model_type";
    return false;
  }
  
  if (out_record.network_input_dim != 5) {
    error_message = std::string("Invalid network_input_dim: expected 5, got ") +
                    std::to_string(out_record.network_input_dim);
    return false;
  }
  
  return true;
}
