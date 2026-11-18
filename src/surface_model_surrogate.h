/* ----------------------------------------------------------------------
   Surface model "surrogate" for surrogate-shape particles (atom_style surrogate)
   Spec 04 R04.2c. Licensed under GPL v2 (LIGGGHTS).

   Selected explicitly in the input, like "surface superquadric":
     pair_style gran model hertz tangential history surface surrogate
     fix ... wall/gran model hertz tangential history surface surrogate primitive ...

   Role: kinematics at the contact point. fix_wall_gran (surrogate block) sets
   sidata.contact_point, sidata.is_non_spherical and the unit normal sidata.en;
   this model computes the relative velocity at the contact point (translation
   + rotation omega x r) and the lever arm, so that the native normal and
   tangential models apply torque = r x F.

   Independent of superquadric code (separate shape family). Uses only the
   generic routine MathExtraLiggghtsNonspherical::surfacesIntersectNonSpherical.

   STOPGAP (Spec 04 R04.2b): the contact point it consumes is currently computed
   in LIGGGHTS from the exact cube geometry; it MUST later come from the
   surrogate model output. This model does not need to change for that.
------------------------------------------------------------------------- */

#ifdef SURFACE_MODEL
SURFACE_MODEL(SURFACE_SURROGATE,surrogate,6)
#else
#ifndef SURFACE_MODEL_SURROGATE_H_
#define SURFACE_MODEL_SURROGATE_H_

#include "surface_model_base.h"

#if defined(SURROGATE_ACTIVE_FLAG) && defined(NONSPHERICAL_ACTIVE_FLAG)

#include "contact_models.h"
#include "atom.h"
#include "math_extra_liggghts_nonspherical.h"

namespace LIGGGHTS {
namespace ContactModels
{
  /**
   * @brief Surface model for surrogate particles (Spec 04 R04.2c)
   *
   * Wall contacts only (ss-ss contact is future, Spec 04 R04.7). The geometric
   * quantities (overlap, contact point, normal) are produced upstream in the
   * surrogate block of fix_wall_gran; this model only turns them into contact
   * kinematics.
   */
  template<>
  class SurfaceModel<SURFACE_SURROGATE> : public SurfaceModelBase
  {
  public:
    /**
     * @brief Construct; error if the atom style is not surrogate
     * @param lmp    LAMMPS instance
     * @param hsetup contact history setup (unused)
     * @param cmb    contact model base (unused)
     */
    SurfaceModel(LAMMPS * lmp, IContactHistorySetup * hsetup, class ContactModelBase *cmb) :
        SurfaceModelBase(lmp, hsetup, cmb)
    {
      if(!atom->surrogate_flag)
        error->one(FLERR,"Surface model 'surrogate' requires atom_style surrogate");
    }

    inline void registerSettings(Settings&) {}
    inline void postSettings(IContactHistorySetup *, ContactModelBase *) {}
    inline void connectToProperties(PropertyRegistry&) {}

    /**
     * @brief Mark the contact as non-spherical (contact point supplied upstream)
     * @param sidata contact data
     * @return always true (the overlap decision is made by the surrogate)
     */
    inline bool checkSurfaceIntersect(SurfacesIntersectData & sidata)
    {
      sidata.is_non_spherical = true;
      return true;
    }

    /**
     * @brief Relative velocity at the contact point and lever arm
     *
     * Requires sidata.contact_point, sidata.en (unit) and sidata.deltan
     * (penetration >= 0), set by fix_wall_gran for surrogate particles.
     *
     * @param sidata contact data (in/out)
     */
    inline void surfacesIntersect(SurfacesIntersectData & sidata, ForceData&, ForceData&)
    {
      if(!sidata.is_wall)
        error->one(FLERR,"Surface model 'surrogate': particle-particle contact not implemented (Spec 04 R04.7)");
      if(!sidata.is_non_spherical)
        error->one(FLERR,"Surface model 'surrogate': contact point not set by the geometry step");
      MathExtraLiggghtsNonspherical::surfacesIntersectNonSpherical(sidata, atom->x);
    }

    inline void endSurfacesIntersect(SurfacesIntersectData&, TriMesh *, double * const) {}
    inline void surfacesClose(SurfacesCloseData&, ForceData&, ForceData&) {}
    void beginPass(SurfacesIntersectData&, ForceData&, ForceData&) {}
    void endPass(SurfacesIntersectData&, ForceData&, ForceData&) {}
    inline void tally_pp(double,int,int,int) {}
    inline void tally_pw(double,int,int,int) {}
  };
}
}

#else

SURFACE_MODEL_DUMMY(SURFACE_SURROGATE, "Surface model 'surrogate' requires a build with ENABLE_SURROGATE=ON")

#endif // SURROGATE_ACTIVE_FLAG && NONSPHERICAL_ACTIVE_FLAG
#endif // SURFACE_MODEL_SURROGATE_H_
#endif
