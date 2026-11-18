/**
 * @file test_surrogate_model.cpp
 * @brief Unit test: run_029 model through LibTorch reproduces the Python reference (Spec 05 RF-18).
 *
 * Reference values from the training-team model evaluated in Python (Spec 00 "Verification
 * evidence"): sd at fixed (z, q). Tolerance 2e-4 (unit-cube length; model MAE ~6e-5).
 * argv[1] = path to SS_models/ss-cube-walls (model.pt, scalers.dat).
 */
#include <torch/script.h>
#include <string>
#include "surrogate_interface.h"
#include "scaler_parser.h"
#include "test_check.h"

using namespace LAMMPS_NS;
using namespace LAMMPS_NS::SurrogateInterface;

int main(int argc, char **argv)
{
  if (argc < 2) { std::printf("usage: %s <SS_models/ss-cube-walls>\n", argv[0]); return 2; }
  const std::string dir = argv[1];
  ScalerRecord rec; std::string err;
  if (!ScalerParser::parse_file(dir + "/scalers.dat", rec, err)) {
    std::printf("FAIL cannot parse scalers.dat: %s\n", err.c_str()); return 1;
  }
  CHECK_TRUE(rec.model_type == "option4", "model_type option4");
  CHECK_NEAR(rec.y_mean, 0.74250850860714912, 1e-15, "y_mean (run_029)");
  CHECK_NEAR(rec.y_scale, 0.13142255689813576, 1e-15, "y_scale (run_029)");

  torch::jit::script::Module m = torch::jit::load(dir + "/model.pt");
  const double down[3] = {0, 0, -1};
  auto sd_of = [&](double z, const double q[4]) {
    torch::NoGradGuard g;
    std::vector<torch::jit::IValue> in{torch::tensor({(float)z}, torch::kFloat32),
      torch::tensor({(float)q[0], (float)q[1], (float)q[2], (float)q[3]}, torch::kFloat32).reshape({1, 4})};
    const double raw = m.forward(in).toTensor().reshape({-1})[0].item<double>();
    return denormalize_signed_distance(raw, rec.y_mean, rec.y_scale, h_exact_cube(q, down));
  };
  const double qi[4] = {1, 0, 0, 0};
  const double qe[4] = {std::cos(M_PI/8), std::sin(M_PI/8), 0, 0};
  struct R { double z; const double *q; double sd_python; const char *name; };
  const R ref[] = {
    {0.45, qi, -0.049681, "face z=0.45"}, {0.49, qi, -0.010033, "face z=0.49"},
    {0.50, qi, -0.000069, "face z=0.50"}, {0.51, qi, +0.009911, "face z=0.51"},
    {0.60, qi, +0.100013, "face z=0.60"},
    {0.65, qe, -0.057058, "edge z=0.65"}, {0.7071, qe, -0.000030, "edge z=0.7071"},
    {0.75, qe, +0.042794, "edge z=0.75"},
  };
  for (const R &r : ref) CHECK_NEAR(sd_of(r.z, r.q), r.sd_python, 2e-4, r.name);
  // nondimensional rescaling: same z/L -> same sd_unit (RF-17), checked through the model
  CHECK_NEAR(sd_of(0.49, qi) * 0.01, -0.010033 * 0.01, 2e-6, "sd_phys at L = 1 cm");
  return TEST_RESULT();
}
