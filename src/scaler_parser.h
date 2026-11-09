/* ----------------------------------------------------------------------
   Parser for ss_model_record_v1 scaler format
   Licensed under GPL v2 (LIGGGHTS)
------------------------------------------------------------------------- */

#ifndef LMP_SCALER_PARSER_H
#define LMP_SCALER_PARSER_H

#include <string>
#include <map>

namespace LAMMPS_NS {

/**
 * @brief Parsed representation of scalers.dat (ss_model_record_v1 format)
 */
struct ScalerRecord {
  // Mandatory fields
  std::string format;              ///< Format version (e.g., "ss_model_record_v1")
  std::string run_id;              ///< Training run identifier
  std::string model_type;          ///< "option4" (physics-informed) or "option1" (pure ANN)
  std::string input_dtype;         ///< Input precision (e.g., "float64")
  std::string input_units;         ///< Unit system (e.g., "canonical_L1")
  double y_mean;                   ///< Output denormalization intercept
  double y_scale;                  ///< Output denormalization scale
  double r_circ;                   ///< Circumradius for shape
  double z_prefilter;              ///< Prefilter threshold
  double sd_too_deep;              ///< Penetration threshold
  std::string h_exact_method;      ///< Method for h_exact computation
  int network_input_dim;           ///< Number of input features (must be 5)
  std::string ref_eval;            ///< Reference evaluation mode
  double ref_sd_tol;               ///< Reference tolerance
  int n_ref;                       ///< Number of reference test cases
  
  // Optional fields (stored in map for forward compatibility)
  std::map<std::string, std::string> optional_fields;
};

/**
 * @brief Parser for ss_model_record_v1 format scalers files
 * 
 * Reads ASCII key-value format from scalers.dat.
 * One key-value pair per line: "key value"
 */
class ScalerParser {
 public:
  /**
   * @brief Parse scalers.dat file
   * 
   * @param filepath Full path to scalers.dat
   * @param out_record Output structure with parsed values
   * @param error_message If parsing fails, contains diagnostic message
   * @return true if parse succeeded, false otherwise
   */
  static bool parse_file(const std::string& filepath,
                        ScalerRecord& out_record,
                        std::string& error_message);

 private:
  /**
   * @brief Parse a single line of key-value pair
   * 
   * @param line Input line
   * @param key Output: key name
   * @param value Output: value string
   * @return true if valid key-value pair, false if empty or comment
   */
  static bool parse_line(const std::string& line,
                        std::string& key,
                        std::string& value);

  /**
   * @brief Convert string to double
   * 
   * @param str String to parse
   * @param out_value Output: parsed double
   * @return true if parse succeeded
   */
  static bool string_to_double(const std::string& str, double& out_value);

  /**
   * @brief Convert string to int
   * 
   * @param str String to parse
   * @param out_value Output: parsed int
   * @return true if parse succeeded
   */
  static bool string_to_int(const std::string& str, int& out_value);
};

} // namespace LAMMPS_NS

#endif /* LMP_SCALER_PARSER_H */
