#ifndef Uintah_Component_Arches_DORadiation_h
#define Uintah_Component_Arches_DORadiation_h
#include <Core/ProblemSpec/ProblemSpec.h>
#include <CCA/Components/Arches/ArchesLabel.h>
#include <Core/Grid/SimulationStateP.h>
#include <CCA/Components/Arches/SourceTerms/SourceTermBase.h>
#include <CCA/Components/Arches/SourceTerms/SourceTermFactory.h>

/** 
* @class  DORadiation
* @author Jeremy Thornock
* @date   August 2011
* 
* @brief Computes the divergence of heat flux contribution from the 
*         solution of the intensity equation. 
*
* The input file interface for this property should like this in your UPS file: 
*
*	 <calc_frequency 						   spec="OPTIONAL INTEGER" need_applies_to="type do_radiation" /> <!-- calculate radiation every N steps, default = 3 --> 
*  <calc_on_all_RKsteps          spec="OPTIONAL BOOLEAN" need_applies_to="type do_radiation" /> <!-- calculate radiation every RK step, default = false --> 
*	 <co2_label 									 spec="OPTIONAL STRING"  need_applies_to="type do_radiation" /> <!-- string label with default of CO2, default = CO2 --> 
*	 <h2o_label 									 spec="OPTIONAL STRING"  need_applies_to="type do_radiation" /> <!-- string label wtih default of H2O, default = H2O --> 
*	 <DORadiationModel 					   spec="REQUIRED NO_DATA" need_applies_to="type do_radiation" >
*  	 <opl                        spec="REQUIRED DOUBLE" />
*    <ordinates                  spec="OPTIONAL INTEGER" />
*    <property_model             spec="OPTIONAL STRING 'radcoef, patchmean, wsggm'" />
*    <LinearSolver               spec="OPTIONAL NO_DATA" 
*            	                        attribute1="type REQUIRED STRING 'hypre, petsc'">
*      <res_tol                  spec="REQUIRED DOUBLE" />
*      <ksptype                  spec="REQUIRED STRING 'gmres, cg'" />
*      <pctype                   spec="REQUIRED STRING 'jacobi, blockjacobi'" />
*      <max_iter                 spec="REQUIRED INTEGER" />
*    </LinearSolver>
*  </DORadiationModel>
*
* TO DO'S: 
*  @todo Remove _bc from code.  But first must remove it from DORadiationModel.cc
*  
*/ 

namespace Uintah{

  class RadiationModel; 
  class ArchesLabel; 
  class BoundaryCondition; 

class DORadiation: public SourceTermBase {
public: 

  DORadiation( std::string srcName, ArchesLabel* labels, BoundaryCondition* bc, 
                vector<std::string> reqLabelNames, const ProcessorGroup* my_world );
  ~DORadiation();

  void problemSetup(const ProblemSpecP& db);
  void sched_computeSource( const LevelP& level, SchedulerP& sched, 
                            int timeSubStep );
  void computeSource( const ProcessorGroup* pc, 
                      const PatchSubset* patches, 
                      const MaterialSubset* matls, 
                      DataWarehouse* old_dw, 
                      DataWarehouse* new_dw, 
                      int timeSubStep );
  void sched_dummyInit( const LevelP& level, SchedulerP& sched );
  void dummyInit( const ProcessorGroup* pc, 
                  const PatchSubset* patches, 
                  const MaterialSubset* matls, 
                  DataWarehouse* old_dw, 
                  DataWarehouse* new_dw );

  class Builder
    : public SourceTermBase::Builder { 

    public: 

      Builder( std::string name, vector<std::string> required_label_names, ArchesLabel* labels, 
          BoundaryCondition* bc, const ProcessorGroup* my_world ) 
        : _name(name), _labels(labels), _bc(bc), _my_world(my_world), _required_label_names(required_label_names){};
      ~Builder(){}; 

      DORadiation* build()
      { return scinew DORadiation( _name, _labels, _bc, _required_label_names, _my_world ); };

    private: 

      std::string _name; 
      ArchesLabel* _labels; 
      BoundaryCondition* _bc; 
      const ProcessorGroup* _my_world; 
      vector<std::string> _required_label_names; 

  }; // class Builder 

private:

  int _radiation_calc_freq; 

  bool _all_rk; 

  std::string _co2_label_name; 
  std::string _h2o_label_name; 
  std::string _T_label_name; 

  const ProcessorGroup* _my_world;
  RadiationModel* _DO_model; 
  BoundaryCondition* _bc; 
  ArchesLabel* _labels; 

  const VarLabel* _co2_label; 
  const VarLabel* _h2o_label; 
  const VarLabel* _T_label; 
  const VarLabel* _abskgLabel;
  const VarLabel* _abskpLabel;
  const VarLabel* _radiationSRCLabel;
  const VarLabel* _radiationFluxELabel;
  const VarLabel* _radiationFluxWLabel;
  const VarLabel* _radiationFluxNLabel;
  const VarLabel* _radiationFluxSLabel;
  const VarLabel* _radiationFluxTLabel;
  const VarLabel* _radiationFluxBLabel;
  const VarLabel* _radiationVolqLabel;
  const PatchSet* _perproc_patches;

}; // end DORadiation
} // end namespace Uintah
#endif
