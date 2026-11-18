/* ----------------------------------------------------------------------
    This is the

    ██╗     ██╗ ██████╗  ██████╗  ██████╗ ██╗  ██╗████████╗███████╗
    ██║     ██║██╔════╝ ██╔════╝ ██╔════╝ ██║  ██║╚══██╔══╝██╔════╝
    ██║     ██║██║  ███╗██║  ███╗██║  ███╗███████║   ██║   ███████╗
    ██║     ██║██║   ██║██║   ██║██║   ██║██╔══██║   ██║   ╚════██║
    ███████╗██║╚██████╔╝╚██████╔╝╚██████╔╝██║  ██║   ██║   ███████║
    ╚══════╝╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝®

    DEM simulation engine, released by
    DCS Computing Gmbh, Linz, Austria
    http://www.dcs-computing.com, office@dcs-computing.com

    LIGGGHTS® is part of CFDEM®project:
    http://www.liggghts.com | http://www.cfdem.com

    Core developer and main author:
    Christoph Kloss, christoph.kloss@dcs-computing.com

    LIGGGHTS® is open-source, distributed under the terms of the GNU Public
    License, version 2 or later. It is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
    received a copy of the GNU General Public License along with LIGGGHTS®.
    If not, see http://www.gnu.org/licenses . See also top-level README
    and LICENSE files.

    LIGGGHTS® and CFDEM® are registered trade marks of DCS Computing GmbH,
    the producer of the LIGGGHTS® software and the CFDEM®coupling software
    See http://www.cfdem.com/terms-trademark-policy for details.

-------------------------------------------------------------------------
    Contributing author and copyright for this file:
    This file is from LAMMPS, but has been modified. Copyright for
    modification:

    Copyright 2012-     DCS Computing GmbH, Linz
    Copyright 2009-2012 JKU Linz

    Copyright of original file:
    LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
    http://lammps.sandia.gov, Sandia National Laboratories
    Steve Plimpton, sjplimp@sandia.gov

    Copyright (2003) Sandia Corporation.  Under the terms of Contract
    DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
    certain rights in this software.  This software is distributed under
    the GNU General Public License.
------------------------------------------------------------------------- */

#include <cmath>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fix_nve_sphere.h"
#include "atom.h"
#include "atom_vec.h"
#include "update.h"
#include "respa.h"
#include "force.h"
#include "error.h"
#include "domain.h" 

using namespace LAMMPS_NS;
using namespace FixConst;

#define INERTIA 0.4          // moment of inertia prefactor for sphere

enum{NONE,DIPOLE};

/* ---------------------------------------------------------------------- */

FixNVESphere::FixNVESphere(LAMMPS *lmp, int narg, char **arg) :
  FixNVE(lmp, narg, arg),
  useAM_(false),
  CAddRhoFluid_(0.0),
  onePlusCAddRhoFluid_(1.0)
{
  if (narg < 3) error->all(FLERR,"Illegal fix nve/sphere command");

  time_integrate = 1;

  // process extra keywords

  extra = NONE;

  int iarg = 3;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"update") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal fix nve/sphere command");
      if (strcmp(arg[iarg+1],"dipole") == 0) extra = DIPOLE;
      else if (strcmp(arg[iarg+1],"CAddRhoFluid") == 0)
      {
            if(narg < iarg+2)
                error->fix_error(FLERR,this,"not enough arguments for 'CAddRhoFluid'");
            iarg+=2;
            useAM_ = true;
            CAddRhoFluid_        = atof(arg[iarg]);
            onePlusCAddRhoFluid_ = 1.0 + CAddRhoFluid_;
            fprintf(screen,"cfd_coupling_force_implicit will consider added mass with CAddRhoFluid = %f\n",
                    CAddRhoFluid_);
      }
      else error->all(FLERR,"Illegal fix nve/sphere command");
      iarg += 2;
    } else error->all(FLERR,"Illegal fix nve/sphere command");
  }

  // error checks

  if (!atom->sphere_flag
#ifdef SURROGATE_ACTIVE_FLAG
      && !atom->surrogate_flag
#endif
  )
    error->all(FLERR,"Fix nve/sphere requires atom style sphere or surrogate");
  if (extra == DIPOLE && !atom->mu_flag)
    error->all(FLERR,"Fix nve/sphere requires atom attribute mu");
}


#ifdef SURROGATE_ACTIVE_FLAG
/**
 * @brief Moment-of-inertia denominator I_i for a surrogate atom (Spec 04 R04.2e)
 *
 * I = k_I * m * L^2 with k_I from the per-shape geometry layer (cube: 1/6).
 * Isotropic shapes only; anisotropic shapes need a body-frame integrator.
 *
 * @param atom LAMMPS atom data
 * @param i    local atom index
 * @return     moment of inertia
 */
static inline double surrogate_inertia(const Atom *atom, int i)
{
  const int key = atom->shape_key[i];
  const double kI = atom->surrogate_shape_inertia_factor[key];
  const double L = atom->characteristic_length[i];
  return kI * atom->rmass[i] * L * L;
}

/**
 * @brief Rotate q by omega over dt: q <- dq(omega dt) (x) q, normalized (Spec 04 R04.2e)
 *
 * Same algorithm as MathExtraLiggghtsNonspherical::integrate_quat (space-frame
 * omega, body-to-world quaternion [qw,qx,qy,qz]); reimplemented here so that
 * surrogate code does not depend on superquadric sources.
 *
 * @param q     quaternion (in/out)
 * @param omega angular velocity, world frame
 * @param dt    time step
 */
static inline void surrogate_integrate_quat(double *q, const double *omega, double dt)
{
  const double wmag = sqrt(omega[0]*omega[0] + omega[1]*omega[1] + omega[2]*omega[2]);
  double dq[4];
  if (wmag > 1e-12) {
    const double s = sin(0.5*wmag*dt) / wmag;
    dq[0] = cos(0.5*wmag*dt);
    dq[1] = s*omega[0]; dq[2] = s*omega[1]; dq[3] = s*omega[2];
  } else {
    dq[0] = 1.0;
    dq[1] = 0.5*dt*omega[0]; dq[2] = 0.5*dt*omega[1]; dq[3] = 0.5*dt*omega[2];
  }
  const double r[4] = {
    dq[0]*q[0] - dq[1]*q[1] - dq[2]*q[2] - dq[3]*q[3],
    dq[0]*q[1] + dq[1]*q[0] + dq[2]*q[3] - dq[3]*q[2],
    dq[0]*q[2] - dq[1]*q[3] + dq[2]*q[0] + dq[3]*q[1],
    dq[0]*q[3] + dq[1]*q[2] - dq[2]*q[1] + dq[3]*q[0] };
  const double n = sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3]);
  for (int k = 0; k < 4; k++) q[k] = r[k] / n;
}
#endif

/* ---------------------------------------------------------------------- */

void FixNVESphere::init()
{
  FixNVE::init();

  // check that all particles are finite-size spheres
  // no point particles allowed

  double *radius = atom->radius;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit)
      if (radius[i] == 0.0)
        error->one(FLERR,"Fix nve/sphere requires extended particles");
}

/* ---------------------------------------------------------------------- */

void FixNVESphere::initial_integrate(int vflag)
{
  double dtfm,dtirotate,msq,scale;
  double g[3];

  double **x = atom->x;
  double **v = atom->v;
  double **f = atom->f;
  double **omega = atom->omega;
  double **torque = atom->torque;
  double *radius = atom->radius;
  double *rmass = atom->rmass;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;

  // set timestep here since dt may have changed or come via rRESPA

  double dtfrotate; 
  if (domain->dimension == 2) dtfrotate = dtf / 0.5; // for discs the formula is I=0.5*Mass*Radius^2
  else dtfrotate  = dtf / INERTIA;

  // update 1/2 step for v and omega, and full step for  x for all particles
  // d_omega/dt = torque / inertia

  for (int i = 0; i < nlocal; i++) {
    if (mask[i] & groupbit) {

      // velocity update for 1/2 step
      dtfm = dtf / (rmass[i]*onePlusCAddRhoFluid_);
      v[i][0] += dtfm * f[i][0];
      v[i][1] += dtfm * f[i][1];
      v[i][2] += dtfm * f[i][2];

      // position update
      x[i][0] += dtv * v[i][0];
      x[i][1] += dtv * v[i][1];
      x[i][2] += dtv * v[i][2];
      
      // rotation update
#ifdef SURROGATE_ACTIVE_FLAG
      if (atom->surrogate_flag) {
        // Spec 04 R04.2e: shape inertia (isotropic) + orientation update
        const double dti = dtf / surrogate_inertia(atom, i);
        omega[i][0] += dti * torque[i][0];
        omega[i][1] += dti * torque[i][1];
        omega[i][2] += dti * torque[i][2];
        surrogate_integrate_quat(atom->quaternion[i], omega[i], dtv);
        continue;
      }
#endif
      dtirotate = dtfrotate / (radius[i]*radius[i]*rmass[i]);
      omega[i][0] += dtirotate * torque[i][0];
      omega[i][1] += dtirotate * torque[i][1];
      omega[i][2] += dtirotate * torque[i][2];
    }
  }

  // update mu for dipoles
  // d_mu/dt = omega cross mu
  // renormalize mu to dipole length

  if (extra == DIPOLE) {
    double **mu = atom->mu;
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & groupbit)
        if (mu[i][3] > 0.0) {
          g[0] = mu[i][0] + dtv * (omega[i][1]*mu[i][2]-omega[i][2]*mu[i][1]);
          g[1] = mu[i][1] + dtv * (omega[i][2]*mu[i][0]-omega[i][0]*mu[i][2]);
          g[2] = mu[i][2] + dtv * (omega[i][0]*mu[i][1]-omega[i][1]*mu[i][0]);
          msq = g[0]*g[0] + g[1]*g[1] + g[2]*g[2];
          scale = mu[i][3]/sqrt(msq);
          mu[i][0] = g[0]*scale;
          mu[i][1] = g[1]*scale;
          mu[i][2] = g[2]*scale;
        }
  }
}

/* ---------------------------------------------------------------------- */

void FixNVESphere::final_integrate()
{
  double dtfm,dtirotate;

  double **v = atom->v;
  double **f = atom->f;
  double **omega = atom->omega;
  double **torque = atom->torque;
  double *rmass = atom->rmass;
  double *radius = atom->radius;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;

  // set timestep here since dt may have changed or come via rRESPA

  double dtfrotate; 
  if (domain->dimension == 2) dtfrotate = dtf / 0.5; // for discs the formula is I=0.5*Mass*Radius^2
  else dtfrotate  = dtf / INERTIA;

  // update 1/2 step for v,omega for all particles
  // d_omega/dt = torque / inertia

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {

      // velocity update for 1/2 step
      dtfm = dtf / (rmass[i]*onePlusCAddRhoFluid_);
      v[i][0] += dtfm * f[i][0];
      v[i][1] += dtfm * f[i][1];
      v[i][2] += dtfm * f[i][2];

      // rotation update
#ifdef SURROGATE_ACTIVE_FLAG
      if (atom->surrogate_flag) {
        // Spec 04 R04.2e: shape inertia (isotropic)
        const double dti = dtf / surrogate_inertia(atom, i);
        omega[i][0] += dti * torque[i][0];
        omega[i][1] += dti * torque[i][1];
        omega[i][2] += dti * torque[i][2];
        continue;
      }
#endif
      dtirotate = dtfrotate / (radius[i]*radius[i]*rmass[i]);
      omega[i][0] += dtirotate * torque[i][0];
      omega[i][1] += dtirotate * torque[i][1];
      omega[i][2] += dtirotate * torque[i][2];
    }
}
