/* ----------------------------------------------------------------------
   compute erotate/surrogate — see compute_erotate_surrogate.h
   Spec 04 R04.2e (RF-13). Licensed under GPL v2 (LIGGGHTS).
------------------------------------------------------------------------- */

#ifdef SURROGATE_ACTIVE_FLAG
#include <mpi.h>
#include "compute_erotate_surrogate.h"
#include "atom.h"
#include "update.h"
#include "force.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

ComputeERotateSurrogate::ComputeERotateSurrogate(LAMMPS *lmp, int &iarg, int narg, char **arg) :
  Compute(lmp, iarg, narg, arg)
{
  if (narg != iarg) error->all(FLERR,"Illegal compute erotate/surrogate command");
  if (!atom->surrogate_flag)
    error->all(FLERR,"Compute erotate/surrogate requires atom_style surrogate");

  scalar_flag = 1;
  extscalar = 1;
  pfactor = 0.5;
}

/* ---------------------------------------------------------------------- */

void ComputeERotateSurrogate::init()
{
  pfactor = 0.5 * force->mvv2e;
}

/* ---------------------------------------------------------------------- */

double ComputeERotateSurrogate::compute_scalar()
{
  invoked_scalar = update->ntimestep;

  double **omega = atom->omega;
  double *rmass = atom->rmass;
  double *L = atom->characteristic_length;
  int *key = atom->shape_key;
  int *mask = atom->mask;
  const int nlocal = atom->nlocal;
  const int nshapes = (int)atom->surrogate_shape_inertia_factor.size();

  double erot = 0.0;
  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      if (key[i] < 0 || key[i] >= nshapes)
        error->one(FLERR,"compute erotate/surrogate: invalid shape key");
      const double kI = atom->surrogate_shape_inertia_factor[key[i]];
      erot += kI * rmass[i] * L[i] * L[i] *
              (omega[i][0]*omega[i][0] + omega[i][1]*omega[i][1] + omega[i][2]*omega[i][2]);
    }

  MPI_Allreduce(&erot,&scalar,1,MPI_DOUBLE,MPI_SUM,world);
  scalar *= pfactor;
  return scalar;
}
#endif
