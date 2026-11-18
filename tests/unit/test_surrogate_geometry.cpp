/**
 * @file test_surrogate_geometry.cpp
 * @brief Unit tests of the surrogate geometry layer (Spec 05 RF-15, RF-16, RF-17).
 *
 * Covers: quaternion canonicalization, exact support height h_exact (Spec 00 R00.3),
 * denormalization sd = out*y_scale + y_mean - h, rescaling by L, the contact-point
 * STOPGAP (Spec 04 R04.2b lever-arm table) and the shape-geometry registry.
 * No LibTorch needed (header-only functions).
 */
#include "surrogate_interface.h"
#include "test_check.h"

using namespace LAMMPS_NS::SurrogateInterface;

static void normalized(double q[4], double w, double x, double y, double z)
{
  const double n = std::sqrt(w*w + x*x + y*y + z*z);
  q[0] = w/n; q[1] = x/n; q[2] = y/n; q[3] = z/n;
}

int main()
{
  const double down[3] = {0.0, 0.0, -1.0};
  const double tol = 1e-9;

  // --- canonicalization (RF-15): q and -q give the same canonical quaternion
  {
    double a[4], b[4];
    normalized(a, 0.9, 0.3, 0.2, 0.1);
    for (int k = 0; k < 4; k++) b[k] = -a[k];
    canonicalize_quaternion(a); canonicalize_quaternion(b);
    for (int k = 0; k < 4; k++) CHECK_NEAR(b[k], a[k], 0.0, "canonical(q) == canonical(-q)");
    CHECK_TRUE(a[0] >= 0.0, "canonical qw >= 0");
  }

  // --- h_exact (support height of the unit cube)
  double q[4];
  normalized(q, 1, 0, 0, 0);
  CHECK_NEAR(h_exact_cube(q, down), 0.5, tol, "h face down");
  normalized(q, std::cos(M_PI/8), std::sin(M_PI/8), 0, 0);
  CHECK_NEAR(h_exact_cube(q, down), std::sqrt(2.0)/2, tol, "h edge (45 deg about x)");
  {
    const double t = std::acos(1/std::sqrt(3.0));
    normalized(q, std::cos(t/2), std::sin(t/2)/std::sqrt(2.0), -std::sin(t/2)/std::sqrt(2.0), 0);
    CHECK_NEAR(h_exact_cube(q, down), std::sqrt(3.0)/2, tol, "h corner (body diagonal vertical)");
  }
  normalized(q, 0.9, 0.3, 0.2, 0.1);
  CHECK_NEAR(h_exact_cube(q, down), 0.82632, 1e-5, "h generic tilt (Spec 04 table)");

  // --- denormalization (RF-16), run_029 scalers
  const double ym = 0.74250850860714912, ys = 0.13142255689813576;
  CHECK_NEAR(denormalize_signed_distance(0.0, ym, ys, 0.5), ym - 0.5, tol, "denorm(0) = y_mean - h");
  CHECK_NEAR(denormalize_signed_distance(1.0, ym, ys, 0.5), ym + ys - 0.5, tol, "denorm(1) = y_mean + y_scale - h");
  // Python reference raw output at z = 0.50, identity quaternion (Spec 00 verification)
  CHECK_NEAR(denormalize_signed_distance(-1.845780, ym, ys, 0.5), -0.000069, 2e-6, "denorm(raw z=0.5)");

  // --- rescaling (RF-17)
  CHECK_NEAR(rescale_to_physical(-0.01, 0.01), -1e-4, 1e-15, "rescale sd_unit * L");

  // --- contact point STOPGAP (Spec 04 R04.2b table): in-plane lever arm
  double cp[3];
  struct Case { const char *name; double w, x, y, z, lever; };
  const double t = std::acos(1/std::sqrt(3.0));
  const Case cases[] = {
    {"face",          1, 0, 0, 0, 0.0},
    {"edge 45",       std::cos(M_PI/8), std::sin(M_PI/8), 0, 0, 0.0},
    {"corner diag",   std::cos(t/2), std::sin(t/2)/std::sqrt(2.0), -std::sin(t/2)/std::sqrt(2.0), 0, 0.0},
    {"edge 30",       std::cos(M_PI/12), std::sin(M_PI/12), 0, 0, 0.18301},
    {"generic tilt",  0.9, 0.3, 0.2, 0.1, 0.25918},
  };
  for (const Case &c : cases) {
    normalized(q, c.w, c.x, c.y, c.z);
    const bool ok = contact_point_cube(q, down, -0.005, cp);
    CHECK_TRUE(ok, "contact point defined for sd < 0");
    CHECK_NEAR(std::hypot(cp[0], cp[1]), c.lever, 2e-4, c.name);
    // point lies on the wall plane: depth below centre = h - p
    CHECK_NEAR(-cp[2], h_exact_cube(q, down) - 0.005, 1e-12, "contact point on the wall plane");
  }
  CHECK_TRUE(!contact_point_cube(q, down, +0.01, cp), "no contact point for a gap (sd > 0)");

  // --- shape registry
  const ShapeGeometry *g = shape_geometry_for("analytical_rotation_matrix");
  CHECK_TRUE(g != nullptr, "cube geometry registered");
  if (g) {
    CHECK_NEAR(g->bounding_radius, std::sqrt(3.0)/2, tol, "cube bounding radius");
    CHECK_NEAR(g->inertia_factor, 1.0/6.0, tol, "cube inertia factor");
  }
  CHECK_TRUE(shape_geometry_for("unknown_method") == nullptr, "unknown h_exact_method rejected");

  return TEST_RESULT();
}
