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
     (Surrogate Shape support - Phase 3C)

 ------------------------------------------------------------------------- */

#ifdef SURROGATE_ACTIVE_FLAG

#ifdef ATOM_CLASS

AtomStyle(surrogate,AtomVecSurrogate)

#else

#ifndef LMP_ATOM_VEC_SURROGATE_H
#define LMP_ATOM_VEC_SURROGATE_H

#include "atom_vec.h"

namespace LAMMPS_NS {

/**
 * @brief Atom vector class for surrogate shape particles
 * 
 * Particles with position, quaternion orientation, characteristic length,
 * and shape identity. Contact is computed via ML surrogate models.
 */
class AtomVecSurrogate : public AtomVec {
 public:
  AtomVecSurrogate(class LAMMPS *);
  ~AtomVecSurrogate() {}
  
   void init();
   void register_shape(const char *);
   void grow(int);
  void grow_reset();
  void copy(int, int, int);
  
  int pack_comm(int, int *, double *, int, int *);
  int pack_comm_vel(int, int *, double *, int, int *);
  int pack_comm_hybrid(int, int *, double *);
  void unpack_comm(int, int, double *);
  void unpack_comm_vel(int, int, double *);
  int unpack_comm_hybrid(int, int, double *);
  
  int pack_reverse(int, int, double *);
  int pack_reverse_hybrid(int, int, double *);
  void unpack_reverse(int, int *, double *);
  int unpack_reverse_hybrid(int, int *, double *);
  
  int pack_border(int, int *, double *, int, int *);
  int pack_border_vel(int, int *, double *, int, int *);
  int pack_border_hybrid(int, int *, double *);
  void unpack_border(int, int, double *);
  void unpack_border_vel(int, int, double *);
  int unpack_border_hybrid(int, int, double *);
  
  void pack_data(double **);
  void write_data(FILE *, int, double **);
  int pack_exchange(int, double *);
  int unpack_exchange(double *);
  
  int size_restart();
  int pack_restart(int, double *);
  int unpack_restart(double *);
  void create_atom(int, double *);
  void data_atom(double *, imageint, char **);
  void data_vel(double *, char **);
  int pack_data_bonus(double *);
  void unpack_data_bonus(double *);
  void data_atom_bonus(double *, char **);
  
  bigint memory_usage();

 private:
  int DELTA;
};

} // namespace LAMMPS_NS

#endif
#endif

#endif  // Close #ifdef SURROGATE_ACTIVE_FLAG

/* ERROR/WARNING messages:

E: Atom style surrogate requires atom attributes mass and volume

E: Cannot use atom_style surrogate with 3d neighbor lists

Neighbor list for surrogate particles is not implemented in 3d.

E: Insufficient memory for atom style surrogate

E: Surrogate shape initialization failed

*/
