/* ----------------------------------------------------------------------
   compute erotate/surrogate — rotational kinetic energy of surrogate particles
   Spec 04 R04.2e (RF-13). Licensed under GPL v2 (LIGGGHTS).

   E_rot = sum_i 0.5 * k_I(shape_i) * m_i * L_i^2 * |omega_i|^2

   k_I comes from the per-shape geometry layer (surrogate_interface.h,
   ShapeGeometry::inertia_factor; cube: 1/6). Isotropic inertia only.
   Usage:  compute ID group-ID erotate/surrogate
------------------------------------------------------------------------- */

#ifdef SURROGATE_ACTIVE_FLAG

#ifdef COMPUTE_CLASS

ComputeStyle(erotate/surrogate,ComputeERotateSurrogate)

#else

#ifndef LMP_COMPUTE_EROTATE_SURROGATE_H
#define LMP_COMPUTE_EROTATE_SURROGATE_H

#include "compute.h"

namespace LAMMPS_NS {

/**
 * @brief Rotational kinetic energy of surrogate particles (shape-aware inertia)
 */
class ComputeERotateSurrogate : public Compute {
 public:
  /**
   * @brief Construct; requires atom_style surrogate and no extra arguments
   */
  ComputeERotateSurrogate(class LAMMPS *, int &iarg, int, char **);
  ~ComputeERotateSurrogate() {}
  /** @brief Set the energy unit prefactor */
  void init();
  /**
   * @brief Sum of 0.5 * k_I * m * L^2 * |omega|^2 over the group
   * @return rotational energy (energy units)
   */
  double compute_scalar();

 private:
  double pfactor;
};

}

#endif
#endif
#endif
