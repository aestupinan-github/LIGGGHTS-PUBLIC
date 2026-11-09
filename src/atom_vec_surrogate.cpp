/* ----------------------------------------------------------------------
     Surrogate Shape atom vector implementation
     Licensed under GPL v2 (LIGGGHTS)
 ------------------------------------------------------------------------- */

#include <cmath>
#include <cstdlib>
#include <cstring>
#include "atom_vec_surrogate.h"
#include "atom.h"
#include "comm.h"
#include "domain.h"
#include "modify.h"
#include "force.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

#define DELTA 10000

/* ---------------------------------------------------------------------- */

AtomVecSurrogate::AtomVecSurrogate(LAMMPS *lmp) : AtomVec(lmp)
{
  molecular = 0;
  comm_x_only = 0;
  comm_f_only = 0;
  size_forward = 7;
  size_reverse = 3;
  size_border = 7;
  size_velocity = 6;
  size_data_atom = 8;
  size_data_vel = 7;
  xcol_data = 6;

  atom->surrogate_flag = 1;
  atom->radius_flag = atom->rmass_flag = atom->omega_flag = atom->density_flag =
    atom->torque_flag = atom->angmom_flag = 1;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::init()
{
  AtomVec::init();
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::grow(int n)
{
  if (n == 0) n = nmax + DELTA;
  else nmax = n;

  atom->nmax = nmax;

  memory->grow(atom->x, nmax, 3, "atom:x");
  memory->grow(atom->v, nmax, 3, "atom:v");
  memory->grow(atom->f, nmax, 3, "atom:f");
  memory->grow(atom->tag, nmax, "atom:tag");
  memory->grow(atom->type, nmax, "atom:type");
  memory->grow(atom->mask, nmax, "atom:mask");
  memory->grow(atom->image, nmax, "atom:image");

  memory->grow(atom->rmass, nmax, "atom:rmass");
  memory->grow(atom->radius, nmax, "atom:radius");
  memory->grow(atom->density, nmax, "atom:density");

  memory->grow(atom->omega, nmax, 3, "atom:omega");
  memory->grow(atom->angmom, nmax, 3, "atom:angmom");
  memory->grow(atom->torque, nmax, 3, "atom:torque");

  memory->grow(atom->quaternion, nmax, 4, "atom:quaternion");

#ifdef SURROGATE_ACTIVE_FLAG
  memory->grow(atom->characteristic_length, nmax, "atom:characteristic_length");
  memory->grow(atom->shape_key, nmax, "atom:shape_key");
#endif

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      modify->fix[atom->extra_grow[iextra]]->grow_arrays(nmax);
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::grow_reset()
{
  atom->x = atom->v = atom->f = NULL;
  atom->tag = atom->type = atom->mask = atom->image = NULL;
  atom->rmass = atom->radius = atom->density = NULL;
  atom->omega = atom->angmom = atom->torque = NULL;
  atom->quaternion = NULL;
#ifdef SURROGATE_ACTIVE_FLAG
  atom->characteristic_length = NULL;
  atom->shape_key = NULL;
#endif
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::copy(int i, int j, int delflag)
{
  atom->x[j][0] = atom->x[i][0];
  atom->x[j][1] = atom->x[i][1];
  atom->x[j][2] = atom->x[i][2];
  atom->v[j][0] = atom->v[i][0];
  atom->v[j][1] = atom->v[i][1];
  atom->v[j][2] = atom->v[i][2];
  atom->tag[j] = atom->tag[i];
  atom->type[j] = atom->type[i];
  atom->mask[j] = atom->mask[i];
  atom->image[j] = atom->image[i];

  atom->rmass[j] = atom->rmass[i];
  atom->radius[j] = atom->radius[i];
  atom->density[j] = atom->density[i];

  atom->omega[j][0] = atom->omega[i][0];
  atom->omega[j][1] = atom->omega[i][1];
  atom->omega[j][2] = atom->omega[i][2];
  atom->angmom[j][0] = atom->angmom[i][0];
  atom->angmom[j][1] = atom->angmom[i][1];
  atom->angmom[j][2] = atom->angmom[i][2];
  atom->torque[j][0] = atom->torque[i][0];
  atom->torque[j][1] = atom->torque[i][1];
  atom->torque[j][2] = atom->torque[i][2];

  atom->quaternion[j][0] = atom->quaternion[i][0];
  atom->quaternion[j][1] = atom->quaternion[i][1];
  atom->quaternion[j][2] = atom->quaternion[i][2];
  atom->quaternion[j][3] = atom->quaternion[i][3];

#ifdef SURROGATE_ACTIVE_FLAG
  atom->characteristic_length[j] = atom->characteristic_length[i];
  atom->shape_key[j] = atom->shape_key[i];
#endif

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      modify->fix[atom->extra_grow[iextra]]->copy_arrays(i, j, delflag);
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_comm(int n, int *list, double *buf,
                               int pbc_flag, int *pbc)
{
  int i, j, m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = atom->x[j][0];
      buf[m++] = atom->x[j][1];
      buf[m++] = atom->x[j][2];
      buf[m++] = atom->quaternion[j][0];
      buf[m++] = atom->quaternion[j][1];
      buf[m++] = atom->quaternion[j][2];
      buf[m++] = atom->quaternion[j][3];
    }
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_comm_vel(int n, int *list, double *buf,
                                   int pbc_flag, int *pbc)
{
  int i, j, m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = atom->x[j][0];
      buf[m++] = atom->x[j][1];
      buf[m++] = atom->x[j][2];
      buf[m++] = atom->v[j][0];
      buf[m++] = atom->v[j][1];
      buf[m++] = atom->v[j][2];
      buf[m++] = atom->quaternion[j][0];
      buf[m++] = atom->quaternion[j][1];
      buf[m++] = atom->quaternion[j][2];
      buf[m++] = atom->quaternion[j][3];
      buf[m++] = atom->omega[j][0];
      buf[m++] = atom->omega[j][1];
      buf[m++] = atom->omega[j][2];
    }
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_comm_hybrid(int n, int *list, double *buf)
{
  int i, j, m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    buf[m++] = atom->quaternion[j][0];
    buf[m++] = atom->quaternion[j][1];
    buf[m++] = atom->quaternion[j][2];
    buf[m++] = atom->quaternion[j][3];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::unpack_comm(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    atom->x[i][0] = buf[m++];
    atom->x[i][1] = buf[m++];
    atom->x[i][2] = buf[m++];
    atom->quaternion[i][0] = buf[m++];
    atom->quaternion[i][1] = buf[m++];
    atom->quaternion[i][2] = buf[m++];
    atom->quaternion[i][3] = buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::unpack_comm_vel(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    atom->x[i][0] = buf[m++];
    atom->x[i][1] = buf[m++];
    atom->x[i][2] = buf[m++];
    atom->v[i][0] = buf[m++];
    atom->v[i][1] = buf[m++];
    atom->v[i][2] = buf[m++];
    atom->quaternion[i][0] = buf[m++];
    atom->quaternion[i][1] = buf[m++];
    atom->quaternion[i][2] = buf[m++];
    atom->quaternion[i][3] = buf[m++];
    atom->omega[i][0] = buf[m++];
    atom->omega[i][1] = buf[m++];
    atom->omega[i][2] = buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::unpack_comm_hybrid(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    atom->quaternion[i][0] = buf[m++];
    atom->quaternion[i][1] = buf[m++];
    atom->quaternion[i][2] = buf[m++];
    atom->quaternion[i][3] = buf[m++];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_reverse(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    buf[m++] = atom->f[i][0];
    buf[m++] = atom->f[i][1];
    buf[m++] = atom->f[i][2];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_reverse_hybrid(int n, int first, double *buf)
{
  return 0;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::unpack_reverse(int n, int *list, double *buf)
{
  int i, j, m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    atom->f[j][0] += buf[m++];
    atom->f[j][1] += buf[m++];
    atom->f[j][2] += buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::unpack_reverse_hybrid(int n, int *list, double *buf)
{
  return 0;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_border(int n, int *list, double *buf,
                                 int pbc_flag, int *pbc)
{
  int i, j, m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = atom->x[j][0];
      buf[m++] = atom->x[j][1];
      buf[m++] = atom->x[j][2];
      buf[m++] = ubuf(atom->tag[j]).d;
      buf[m++] = ubuf(atom->type[j]).d;
      buf[m++] = ubuf(atom->mask[j]).d;
      buf[m++] = ubuf(atom->image[j]).d;
      buf[m++] = atom->quaternion[j][0];
      buf[m++] = atom->quaternion[j][1];
      buf[m++] = atom->quaternion[j][2];
      buf[m++] = atom->quaternion[j][3];
    }
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_border_vel(int n, int *list, double *buf,
                                     int pbc_flag, int *pbc)
{
  int i, j, m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = atom->x[j][0];
      buf[m++] = atom->x[j][1];
      buf[m++] = atom->x[j][2];
      buf[m++] = ubuf(atom->tag[j]).d;
      buf[m++] = ubuf(atom->type[j]).d;
      buf[m++] = ubuf(atom->mask[j]).d;
      buf[m++] = ubuf(atom->image[j]).d;
      buf[m++] = atom->v[j][0];
      buf[m++] = atom->v[j][1];
      buf[m++] = atom->v[j][2];
      buf[m++] = atom->quaternion[j][0];
      buf[m++] = atom->quaternion[j][1];
      buf[m++] = atom->quaternion[j][2];
      buf[m++] = atom->quaternion[j][3];
      buf[m++] = atom->omega[j][0];
      buf[m++] = atom->omega[j][1];
      buf[m++] = atom->omega[j][2];
    }
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_border_hybrid(int n, int *list, double *buf)
{
  int i, j, m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    buf[m++] = atom->quaternion[j][0];
    buf[m++] = atom->quaternion[j][1];
    buf[m++] = atom->quaternion[j][2];
    buf[m++] = atom->quaternion[j][3];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::unpack_border(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    atom->x[i][0] = buf[m++];
    atom->x[i][1] = buf[m++];
    atom->x[i][2] = buf[m++];
    atom->tag[i] = (tagint) ubuf(buf[m++]).i;
    atom->type[i] = (int) ubuf(buf[m++]).i;
    atom->mask[i] = (int) ubuf(buf[m++]).i;
    atom->image[i] = (imageint) ubuf(buf[m++]).i;
    atom->quaternion[i][0] = buf[m++];
    atom->quaternion[i][1] = buf[m++];
    atom->quaternion[i][2] = buf[m++];
    atom->quaternion[i][3] = buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::unpack_border_vel(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    atom->x[i][0] = buf[m++];
    atom->x[i][1] = buf[m++];
    atom->x[i][2] = buf[m++];
    atom->tag[i] = (tagint) ubuf(buf[m++]).i;
    atom->type[i] = (int) ubuf(buf[m++]).i;
    atom->mask[i] = (int) ubuf(buf[m++]).i;
    atom->image[i] = (imageint) ubuf(buf[m++]).i;
    atom->v[i][0] = buf[m++];
    atom->v[i][1] = buf[m++];
    atom->v[i][2] = buf[m++];
    atom->quaternion[i][0] = buf[m++];
    atom->quaternion[i][1] = buf[m++];
    atom->quaternion[i][2] = buf[m++];
    atom->quaternion[i][3] = buf[m++];
    atom->omega[i][0] = buf[m++];
    atom->omega[i][1] = buf[m++];
    atom->omega[i][2] = buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::unpack_border_hybrid(int n, int first, double *buf)
{
  int i, m, last;
  last = first + n;
  m = 0;
  for (i = first; i < last; i++) {
    atom->quaternion[i][0] = buf[m++];
    atom->quaternion[i][1] = buf[m++];
    atom->quaternion[i][2] = buf[m++];
    atom->quaternion[i][3] = buf[m++];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_exchange(int i, double *buf)
{
  int m = 1;
  buf[m++] = atom->x[i][0];
  buf[m++] = atom->x[i][1];
  buf[m++] = atom->x[i][2];
  buf[m++] = atom->v[i][0];
  buf[m++] = atom->v[i][1];
  buf[m++] = atom->v[i][2];
  buf[m++] = ubuf(atom->tag[i]).d;
  buf[m++] = ubuf(atom->type[i]).d;
  buf[m++] = ubuf(atom->mask[i]).d;
  buf[m++] = ubuf(atom->image[i]).d;
  
  buf[m++] = atom->radius[i];
  buf[m++] = atom->rmass[i];
  buf[m++] = atom->density[i];
  buf[m++] = atom->omega[i][0];
  buf[m++] = atom->omega[i][1];
  buf[m++] = atom->omega[i][2];
  buf[m++] = atom->angmom[i][0];
  buf[m++] = atom->angmom[i][1];
  buf[m++] = atom->angmom[i][2];
  
  buf[m++] = atom->quaternion[i][0];
  buf[m++] = atom->quaternion[i][1];
  buf[m++] = atom->quaternion[i][2];
  buf[m++] = atom->quaternion[i][3];

#ifdef SURROGATE_ACTIVE_FLAG
  buf[m++] = atom->characteristic_length[i];
  buf[m++] = ubuf(atom->shape_key[i]).d;
#endif

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      m += modify->fix[atom->extra_grow[iextra]]->pack_exchange(i,&buf[m]);

  buf[0] = m;
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::unpack_exchange(double *buf)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  int m = 1;
  atom->x[nlocal][0] = buf[m++];
  atom->x[nlocal][1] = buf[m++];
  atom->x[nlocal][2] = buf[m++];
  atom->v[nlocal][0] = buf[m++];
  atom->v[nlocal][1] = buf[m++];
  atom->v[nlocal][2] = buf[m++];
  atom->tag[nlocal] = (tagint) ubuf(buf[m++]).i;
  atom->type[nlocal] = (int) ubuf(buf[m++]).i;
  atom->mask[nlocal] = (int) ubuf(buf[m++]).i;
  atom->image[nlocal] = (imageint) ubuf(buf[m++]).i;

  atom->radius[nlocal] = buf[m++];
  atom->rmass[nlocal] = buf[m++];
  atom->density[nlocal] = buf[m++];
  atom->omega[nlocal][0] = buf[m++];
  atom->omega[nlocal][1] = buf[m++];
  atom->omega[nlocal][2] = buf[m++];
  atom->angmom[nlocal][0] = buf[m++];
  atom->angmom[nlocal][1] = buf[m++];
  atom->angmom[nlocal][2] = buf[m++];

  atom->quaternion[nlocal][0] = buf[m++];
  atom->quaternion[nlocal][1] = buf[m++];
  atom->quaternion[nlocal][2] = buf[m++];
  atom->quaternion[nlocal][3] = buf[m++];

#ifdef SURROGATE_ACTIVE_FLAG
  atom->characteristic_length[nlocal] = buf[m++];
  atom->shape_key[nlocal] = (int) ubuf(buf[m++]).i;
#endif

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      m += modify->fix[atom->extra_grow[iextra]]->unpack_exchange(nlocal,&buf[m]);

  atom->nlocal++;
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::size_restart()
{
  return 14 + 4;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_restart(int i, double *buf)
{
  int m = 1;
  buf[m++] = ubuf(atom->tag[i]).d;
  buf[m++] = ubuf(atom->type[i]).d;
  buf[m++] = ubuf(atom->mask[i]).d;
  buf[m++] = atom->x[i][0];
  buf[m++] = atom->x[i][1];
  buf[m++] = atom->x[i][2];
  buf[m++] = atom->v[i][0];
  buf[m++] = atom->v[i][1];
  buf[m++] = atom->v[i][2];
  buf[m++] = ubuf(atom->image[i]).d;
  buf[m++] = atom->quaternion[i][0];
  buf[m++] = atom->quaternion[i][1];
  buf[m++] = atom->quaternion[i][2];
  buf[m++] = atom->quaternion[i][3];
#ifdef SURROGATE_ACTIVE_FLAG
  buf[m++] = atom->characteristic_length[i];
  buf[m++] = ubuf(atom->shape_key[i]).d;
#endif
  buf[0] = m;
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::unpack_restart(double *buf)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  int m = 1;
  atom->tag[nlocal] = (tagint) ubuf(buf[m++]).i;
  atom->type[nlocal] = (int) ubuf(buf[m++]).i;
  atom->mask[nlocal] = (int) ubuf(buf[m++]).i;
  atom->x[nlocal][0] = buf[m++];
  atom->x[nlocal][1] = buf[m++];
  atom->x[nlocal][2] = buf[m++];
  atom->v[nlocal][0] = buf[m++];
  atom->v[nlocal][1] = buf[m++];
  atom->v[nlocal][2] = buf[m++];
  atom->image[nlocal] = (imageint) ubuf(buf[m++]).i;
  atom->quaternion[nlocal][0] = buf[m++];
  atom->quaternion[nlocal][1] = buf[m++];
  atom->quaternion[nlocal][2] = buf[m++];
  atom->quaternion[nlocal][3] = buf[m++];
#ifdef SURROGATE_ACTIVE_FLAG
  atom->characteristic_length[nlocal] = buf[m++];
  atom->shape_key[nlocal] = (int) ubuf(buf[m++]).i;
#endif
  atom->nlocal++;
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::create_atom(int n, double *buf)
{
  int nlocal = atom->nlocal;
  for (int i = 0; i < n; i++) {
    if (nlocal == nmax) grow(0);
    atom->tag[nlocal] = (tagint) ubuf(buf[0]).i;
    atom->type[nlocal] = (int) ubuf(buf[1]).i;
    if (atom->type[nlocal] <= 0 || atom->type[nlocal] > atom->ntypes)
      error->one(FLERR, "Invalid atom type in create_atoms");
    atom->x[nlocal][0] = buf[2];
    atom->x[nlocal][1] = buf[3];
    atom->x[nlocal][2] = buf[4];
    atom->v[nlocal][0] = 0;
    atom->v[nlocal][1] = 0;
    atom->v[nlocal][2] = 0;
    atom->mask[nlocal] = 1;
    atom->image[nlocal] = ((imageint) ubuf(buf[5]).i << IMG2BITS) |
                          ((imageint) ubuf(buf[6]).i << IMGBITS) |
                          ((imageint) ubuf(buf[7]).i & IMGMASK);

    atom->quaternion[nlocal][0] = 1.0;
    atom->quaternion[nlocal][1] = 0.0;
    atom->quaternion[nlocal][2] = 0.0;
    atom->quaternion[nlocal][3] = 0.0;
    
    atom->omega[nlocal][0] = atom->omega[nlocal][1] = atom->omega[nlocal][2] = 0.0;
    atom->angmom[nlocal][0] = atom->angmom[nlocal][1] = atom->angmom[nlocal][2] = 0.0;

#ifdef SURROGATE_ACTIVE_FLAG
    atom->characteristic_length[nlocal] = 1.0;
    atom->shape_key[nlocal] = 0;
#endif

    nlocal++;
    buf += size_data_atom;
  }
  atom->nlocal = nlocal;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::data_atom(double *buf, imageint image, char **values)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  atom->tag[nlocal] = (tagint) atoll(values[0]);
  atom->type[nlocal] = (int) atoi(values[1]);
  if (atom->type[nlocal] <= 0 || atom->type[nlocal] > atom->ntypes)
    error->one(FLERR, "Invalid atom type in data file");

  atom->x[nlocal][0] = atof(values[2]);
  atom->x[nlocal][1] = atof(values[3]);
  atom->x[nlocal][2] = atof(values[4]);
  atom->image[nlocal] = image;

  atom->v[nlocal][0] = 0;
  atom->v[nlocal][1] = 0;
  atom->v[nlocal][2] = 0;
  atom->mask[nlocal] = 1;

  atom->quaternion[nlocal][0] = 1.0;
  atom->quaternion[nlocal][1] = 0.0;
  atom->quaternion[nlocal][2] = 0.0;
  atom->quaternion[nlocal][3] = 0.0;
  
  atom->omega[nlocal][0] = atom->omega[nlocal][1] = atom->omega[nlocal][2] = 0.0;
  atom->angmom[nlocal][0] = atom->angmom[nlocal][1] = atom->angmom[nlocal][2] = 0.0;

#ifdef SURROGATE_ACTIVE_FLAG
  atom->characteristic_length[nlocal] = 1.0;
  atom->shape_key[nlocal] = 0;
#endif

  atom->nlocal++;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::data_vel(double *buf, char **values)
{
  int nlocal = atom->nlocal - 1;
  atom->v[nlocal][0] = atof(values[1]);
  atom->v[nlocal][1] = atof(values[2]);
  atom->v[nlocal][2] = atof(values[3]);
  atom->omega[nlocal][0] = atof(values[4]);
  atom->omega[nlocal][1] = atof(values[5]);
  atom->omega[nlocal][2] = atof(values[6]);
}

/* ---------------------------------------------------------------------- */

int AtomVecSurrogate::pack_data_bonus(double *buf)
{
  return 0;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::unpack_data_bonus(double *buf)
{
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::data_atom_bonus(double *buf, char **values)
{
}

/* ---------------------------------------------------------------------- */

bigint AtomVecSurrogate::memory_usage()
{
  bigint bytes = 0;
  
  bytes += 3 * nmax * sizeof(double);  // x
  bytes += 3 * nmax * sizeof(double);  // v
  bytes += 3 * nmax * sizeof(double);  // f
  bytes += nmax * sizeof(tagint);      // tag
  bytes += nmax * sizeof(int);         // type
  bytes += nmax * sizeof(int);         // mask
  bytes += nmax * sizeof(imageint);    // image
  
  bytes += nmax * sizeof(double);      // rmass
  bytes += nmax * sizeof(double);      // radius
  bytes += nmax * sizeof(double);      // density
  
  bytes += 3 * nmax * sizeof(double);  // omega
  bytes += 3 * nmax * sizeof(double);  // angmom
  bytes += 3 * nmax * sizeof(double);  // torque
  
  bytes += 4 * nmax * sizeof(double);  // quaternion
  
#ifdef SURROGATE_ACTIVE_FLAG
  bytes += nmax * sizeof(double);      // characteristic_length
  bytes += nmax * sizeof(int);         // shape_key
#endif
  
  return bytes;
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::pack_data(double **buf)
{
  // Minimal implementation; surrogate particles don't have special data
}

/* ---------------------------------------------------------------------- */

void AtomVecSurrogate::write_data(FILE *fp, int n, double **buf)
{
  // Minimal implementation; surrogate particles write standard atom data
}
