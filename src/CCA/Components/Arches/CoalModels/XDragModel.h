#ifndef Uintah_Component_Arches_XDragModel_h
#define Uintah_Component_Arches_XDragModel_h
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Grid/SimulationStateP.h>
#include <CCA/Components/Arches/CoalModels/ModelBase.h>
#include <CCA/Components/Arches/CoalModels/CoalModelFactory.h>

#include <CCA/Components/Arches/ArchesVariables.h>

#include <vector>
#include <string>

#define YDIM
#define ZDIM

//===========================================================================

/**
  * @class    XDragModel
  * @author   Julien Pedel
  * @date     September 2009
  *
  * @brief    A class for calculating the two-way coupling between
  *           particle velocities and the gas phase velocities.
  *
  */

//---------------------------------------------------------------------------

namespace Uintah{

//---------------------------------------------------------------------------
// Builder
class XDragModelBuilder: public ModelBuilder
{
public: 
  XDragModelBuilder( const std::string          & modelName, 
                     const std::vector<std::string>  & reqICLabelNames,
                     const std::vector<std::string>  & reqScalarLabelNames,
                     ArchesLabel          * fieldLabels,
                     SimulationStateP           & sharedState,
                     int qn );
  ~XDragModelBuilder(); 

  ModelBase* build(); 

private:

}; 
// End Builder
//---------------------------------------------------------------------------

class XDragModel: public ModelBase {
public: 

  XDragModel( std::string modelName, 
              SimulationStateP& shared_state,
              ArchesLabel* fieldLabels,
              std::vector<std::string> reqICLabelNames,
              std::vector<std::string> reqScalarLabelNames,
              int qn );

  ~XDragModel();

  ///////////////////////////////////////////////
  // Initialization methods

  /** @brief Interface for the inputfile and set constants */ 
  void problemSetup(const ProblemSpecP& db, int qn);

  /** @brief Schedule the initialization of special/local variables unique to model */
  void sched_initVars( const LevelP& level, SchedulerP& sched );

  /** @brief  Actually initialize special variables unique to model */
  void initVars( const ProcessorGroup * pc, 
                 const PatchSubset    * patches, 
                 const MaterialSubset * matls, 
                 DataWarehouse        * old_dw, 
                 DataWarehouse        * new_dw );

  /////////////////////////////////////////////////
  // Model computation methods

  /** @brief Schedule the calculation of the source term */ 
  void sched_computeModel( const LevelP& level, 
                           SchedulerP& sched, 
                           int timeSubStep );

  /** @brief Actually compute the source term */ 
  void computeModel( const ProcessorGroup* pc, 
                     const PatchSubset* patches, 
                     const MaterialSubset* matls, 
                     DataWarehouse* old_dw, 
                     DataWarehouse* new_dw );

  ///////////////////////////////////////////////
  // Access methods

  inline std::string getType() {
    return "Velocity"; }


private:

  const VarLabel* d_particle_length_label;
  const VarLabel* d_raw_coal_mass_label;
  const VarLabel* d_char_mass_label;
  const VarLabel* d_particle_velocity_label;
  const VarLabel* d_gas_velocity_label;
  const VarLabel* d_weight_label;

  std::vector<double>  rc_mass_init;
  std::vector<double>  ash_mass_init;
  Vector gravity;
  double kvisc;
  double rhop;
  double d_lowModelClip;
  double d_highModelClip;
  double d_pl_scaling_factor;
  double d_rcmass_scaling_factor;
  double d_charmass_scaling_factor;
  double d_pv_scaling_factor;
  double d_w_scaling_factor;
  double d_xvel_scaling_factor;
  double d_w_small; // "small" clip value for zero weights

  double pi;

  Vector cart2sph( Vector X ) {
    // converts cartesean to spherical coords
    double mag   = pow( X.x(), 2.0 );
    double magxy = mag;
    double z = 0;
    double y = 0;
#ifdef YDIM
    mag   += pow( X.y(), 2.0 );
    magxy = mag;
    y = X.y();
#endif
#ifdef ZDIM
    mag += pow( X.z(), 2.0 );
    z = X.z();
#endif

    mag   = pow(mag, 1./2.);
    magxy = pow(magxy, 1./2.);

    double elev = atan2( z, magxy );
    double az   = atan2( y, X.x() );

    Vector answer(az, elev, mag);
    return answer;

  };

  Vector sph2cart( Vector X ) {
    // converts spherical to cartesian coords
    double x = 0.;
    double y = 0.;
    double z = 0.;

    double rcoselev = X.z() * cos(X.y());
    x = rcoselev * cos(X.x());
#ifdef YDIM
    y = rcoselev * sin(X.x());
#endif
#ifdef ZDIM
    z = X.z()*sin(X.y());
#endif
    Vector answer(x,y,z);
    return answer;
  };

};
} // end namespace Uintah
#endif
