#include <Packages/Uintah/CCA/Components/ICE/ICE.h>
#include <Packages/Uintah/CCA/Components/MPMICE/MPMICELabel.h>
#include <Packages/Uintah/CCA/Ports/CFDInterface.h>
#include <Packages/Uintah/Core/Grid/VarLabel.h>
#include <Packages/Uintah/Core/Grid/CCVariable.h>
#include <Core/Geometry/Vector.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/Core/Grid/Array3Index.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/CCVariable.h>
#include <Packages/Uintah/Core/Grid/NCVariable.h>
#include <Packages/Uintah/Core/Grid/ParticleSet.h>
#include <Packages/Uintah/Core/Grid/ParticleVariable.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Grid/NodeIterator.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/PerPatch.h>
#include <Packages/Uintah/Core/Grid/ReductionVariable.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/SoleVariable.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpecP.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <vector>
#include <Packages/Uintah/Core/Grid/BoundCond.h>
#include <Packages/Uintah/Core/Grid/PressureBoundCond.h>
#include <Packages/Uintah/Core/Grid/VelocityBoundCond.h>
#include <Packages/Uintah/Core/Grid/TemperatureBoundCond.h>
#include <Packages/Uintah/Core/Grid/DensityBoundCond.h>
#include <Packages/Uintah/CCA/Components/MPM/SerialMPM.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>

using std::vector;
using std::max;

using namespace SCIRun;
using namespace Uintah;

static int iterNum = 0;
static bool computeDt = false;

ICE::ICE(const ProcessorGroup* myworld) 
  : UintahParallelComponent(myworld)
{
  lb   = scinew ICELabel();
  MIlb = scinew MPMICELabel();

  IFS_CCLabel = scinew VarLabel("IFS_CC",
                                CCVariable<fflux>::getTypeDescription());
  OFS_CCLabel = scinew VarLabel("OFS_CC",
                                CCVariable<fflux>::getTypeDescription());
  IFE_CCLabel = scinew VarLabel("IFE_CC",
                                CCVariable<eflux>::getTypeDescription());
  OFE_CCLabel = scinew VarLabel("OFE_CC",
                                CCVariable<eflux>::getTypeDescription());
  IFC_CCLabel = scinew VarLabel("IFC_CC",
                                CCVariable<cflux>::getTypeDescription());
  OFC_CCLabel = scinew VarLabel("OFC_CC",
                                CCVariable<cflux>::getTypeDescription());
  q_outLabel = scinew VarLabel("q_out",
                                CCVariable<fflux>::getTypeDescription());
  q_out_EFLabel = scinew VarLabel("q_out_EF",
                                CCVariable<eflux>::getTypeDescription());
  q_out_CFLabel = scinew VarLabel("q_out_CF",
                                CCVariable<cflux>::getTypeDescription());
  q_inLabel = scinew VarLabel("q_in",
                                CCVariable<fflux>::getTypeDescription());
  q_in_EFLabel = scinew VarLabel("q_in_EF",
                                CCVariable<eflux>::getTypeDescription());
  q_in_CFLabel = scinew VarLabel("q_in_CF",
                                CCVariable<cflux>::getTypeDescription());

  // Turn off all the debuging switches
  switchDebugInitialize = false;
  switchDebug_equilibration_press = false;
  switchDebug_vel_FC = false;
  switchDebug_Exchange_FC = false;
  switchDebug_explicit_press = false;
  switchDebug_PressFC = false;
  switchDebugLagrangianValues = false;
  switchDebugMomentumExchange_CC = false;
  switchDebugSource_Sink = false;
  switchDebug_advance_advect = false;
  switchDebug_advectQFirst = false;
  
}

ICE::~ICE()
{
  delete lb;
  delete MIlb;
  delete IFS_CCLabel;
  delete OFS_CCLabel;
  delete IFE_CCLabel;
  delete OFE_CCLabel;
  delete IFC_CCLabel;
  delete OFC_CCLabel;
  delete q_outLabel;
  delete q_out_EFLabel;
  delete q_out_CFLabel;
  delete q_inLabel;
  delete q_in_EFLabel;
  delete q_in_CFLabel;

}
/* ---------------------------------------------------------------------
 Function~  ICE::problemSetup--
 Purpose~  Read the inputfile 
_____________________________________________________________________*/
void ICE::problemSetup(const ProblemSpecP& prob_spec,GridP& grid,
		       SimulationStateP&   sharedState)
{
  d_sharedState = sharedState;
  d_SMALL_NUM = 1.e-100;

  cerr << "In the preprocessor . . ." << endl;

  // Find the debug switches
  ProblemSpecP debug_ps = prob_spec->findBlock("Debug");
  if (debug_ps) {
    for (ProblemSpecP child = debug_ps->findBlock("debug"); child != 0;
	 child = child->findNextBlock("debug")) {
      map<string,string> debug_attr;
      child->getAttributes(debug_attr);
      if (debug_attr["label"]      == "switchDebugInitialize")
	switchDebugInitialize            = true;
      else if (debug_attr["label"] == "switchDebug_equilibration_press")
	switchDebug_equilibration_press  = true;
      else if (debug_attr["label"] == "switchDebug_vel_FC")
	switchDebug_vel_FC               = true;
      else if (debug_attr["label"] == "switchDebug_Exchange_FC")
	switchDebug_Exchange_FC          = true;
      else if (debug_attr["label"] == "switchDebug_explicit_press")
	switchDebug_explicit_press       = true;
      else if (debug_attr["label"] == "switchDebug_PressFC")
	switchDebug_PressFC              = true;
      else if (debug_attr["label"] == "switchDebugLagrangianValues")
	switchDebugMomentumExchange_CC   = true;
      else if (debug_attr["label"] == "switchDebugMomentumExchange_CC")
	switchDebugLagrangianValues      = true;
      else if (debug_attr["label"] == "switchDebugSource_Sink")
	switchDebugSource_Sink           = true;
      else if (debug_attr["label"] == "switchDebug_advance_advect")
	switchDebug_advance_advect       = true;
      else if (debug_attr["label"] == "switchDebug_advectQFirst")
	switchDebug_advectQFirst         = true;
    }
  }
  cerr << "Pulled out the debugging switches from input file" << endl;

  // Pull out from CFD-ICE section
  ProblemSpecP cfd_ps = prob_spec->findBlock("CFD");
  cfd_ps->require("cfl",d_CFL);
  ProblemSpecP cfd_ice_ps = cfd_ps->findBlock("ICE");
  cfd_ice_ps->require("max_iteration_equilibration",d_max_iter_equilibration);
  cerr << "Pulled out CFD-ICE block of the input file" << endl;
    
  // Pull out from Time section
  ProblemSpecP time_ps = prob_spec->findBlock("Time");
  time_ps->require("delt_init",d_initialDt);
  cerr << "Pulled out Time block of the input file" << endl;

  // Pull out Initial Conditions
  ProblemSpecP mat_ps       =  prob_spec->findBlock("MaterialProperties");
  ProblemSpecP ice_mat_ps   = mat_ps->findBlock("ICE");  

  for (ProblemSpecP ps = ice_mat_ps->findBlock("material"); ps != 0;
       ps = ps->findNextBlock("material") ) {
    // Extract out the type of EOS and the 
    // associated parameters
     ICEMaterial *mat = scinew ICEMaterial(ps);
     sharedState->registerICEMaterial(mat);
  }     
  cerr << "Pulled out InitialConditions block of the input file" << endl;
  
  // Pull out the exchange coefficients
  ProblemSpecP exch_ps = ice_mat_ps->findBlock("exchange_coefficients");
  exch_ps->require("momentum",d_K_mom);
  exch_ps->require("heat",d_K_heat);
  cerr << "Pulled out exchange coefficients of the input file" << endl;

//__________________________________
//  Print out what I've found
  cout << "cfl = " << d_CFL << endl;
  cout << "max_iteration_equilibration " << d_max_iter_equilibration << endl;
  cout << "Initial dt = " << d_initialDt << endl;
  cout << "Number of ICE materials: " 
       << d_sharedState->getNumICEMatls()<< endl;
  
  for (int i = 0; i<(int)d_K_mom.size(); i++)
    cout << "K_mom = " << d_K_mom[i] << endl;
  for (int i = 0; i<(int)d_K_heat.size(); i++)
    cout << "K_heat = " << d_K_heat[i] << endl;

  if (switchDebugInitialize == true) 
    cout << "switchDebugInitialize is ON" << endl;
  if (switchDebug_equilibration_press == true) 
    cout << "switchDebug_equilibration_press is ON" << endl;
  if (switchDebug_vel_FC == true) 
    cout << "switchDebug_vel_FC is ON" << endl;
  if (switchDebug_Exchange_FC == true) 
    cout << "switchDebug_Exchange_FC is ON" << endl;
  if (switchDebug_explicit_press == true) 
    cout << "switchDebug_explicit_press is ON" << endl;
  if (switchDebug_PressFC == true) 
    cout << "switchDebug_PressFC is ON" << endl;
  if (switchDebugLagrangianValues == true) 
    cout << "switchDebugLagrangianValues is ON" << endl;
  if (switchDebugSource_Sink == true) 
    cout << "switchDebugSource_Sink is ON" << endl;
  if (switchDebug_advance_advect == true) 
    cout << "switchDebug_advance_advect is ON" << endl;
  if (switchDebug_advectQFirst == true) 
    cout << "switchDebug_advectQFirst is ON" << endl;

}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleInitialize--
 Purpose~  Initialize all the variables
_____________________________________________________________________*/
void ICE::scheduleInitialize(const LevelP& level, SchedulerP& sched,
			     DataWarehouseP& dw)       
{
  Level::const_patchIterator iter;

  for(iter=level->patchesBegin(); iter != level->patchesEnd(); iter++){
    const Patch* patch=*iter;
    Task* t = scinew Task("ICE::actuallyInitialize", patch, dw, dw,this,
			  &ICE::actuallyInitialize);
    t->computes( dw,    d_sharedState->get_delt_label());
    
    for (int m = 0; m < d_sharedState->getNumICEMatls(); m++ ) {
      ICEMaterial*  matl = d_sharedState->getICEMaterial(m);
      int dwindex = matl->getDWIndex();
      
      t->computes( dw, lb->temp_CCLabel,      dwindex, patch);
      t->computes( dw, lb->rho_micro_CCLabel, dwindex, patch);
      t->computes( dw, lb->rho_CCLabel,       dwindex, patch);
      t->computes( dw, lb->cv_CCLabel,        dwindex, patch);
      t->computes( dw, lb->viscosity_CCLabel, dwindex, patch);
      t->computes( dw, lb->vol_frac_CCLabel,  dwindex, patch);
      t->computes( dw, lb->vel_CCLabel,       dwindex, patch);
      t->computes( dw, lb->uvel_FCLabel,      dwindex, patch);
      t->computes( dw, lb->vvel_FCLabel,      dwindex, patch);
      t->computes( dw, lb->wvel_FCLabel,      dwindex, patch);
    }
    
    t->computes(dw, lb->press_CCLabel,0, patch);

    sched->addTask(t);
  }
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeStableTimestep--
 Purpose~  Compute the next timestep
_____________________________________________________________________*/
void ICE::scheduleComputeStableTimestep(const LevelP& level,SchedulerP& sched,
					DataWarehouseP& dw)
{
    int numMatls = d_sharedState->getNumICEMatls();
    
    for (Level::const_patchIterator iter = level->patchesBegin();
	 iter != level->patchesEnd(); iter++)  {
      const Patch* patch = *iter;
      
      Task* task = scinew Task("ICE::actuallyComputeStableTimestep",patch, dw,
			       dw,this, &ICE::actuallyComputeStableTimestep);
      if(computeDt) {      
	for (int m = 0; m < numMatls; m++) {
	  ICEMaterial* matl = d_sharedState->getICEMaterial(m);
	  int dwindex = matl->getDWIndex();
	  task->requires(dw,lb->vel_CCLabel, dwindex,  patch,  Ghost::None);
	  task->requires(dw,lb->speedSound_CCLabel, dwindex,patch,Ghost::None);
	}
      }
      task->computes(dw, d_sharedState->get_delt_label());
      sched->addTask(task);
    }
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleTimeAdvance--
 Purpose~  Schedule the main loop for ICE
_____________________________________________________________________*/
void ICE::scheduleTimeAdvance(double t, double dt,const LevelP& level,
			      SchedulerP& sched, DataWarehouseP& old_dw,
			      DataWarehouseP& new_dw)
{
  for(Level::const_patchIterator iter=level->patchesBegin();
       iter != level->patchesEnd(); iter++)  {
    const Patch* patch=*iter;
    
    scheduleComputeEquilibrationPressure( 
        patch,  sched,  old_dw, new_dw);
        
    scheduleComputeFaceCenteredVelocities( 
        patch,  sched,  old_dw, new_dw);
    
    scheduleAddExchangeContributionToFCVel( 
        patch,  sched,  old_dw, new_dw);
    
    scheduleComputeDelPressAndUpdatePressCC(  
        patch,  sched,  old_dw, new_dw);
    
    scheduleComputePressFC(  
        patch,  sched,  old_dw, new_dw);
    
    scheduleAccumulateMomentumSourceSinks( 
        patch,  sched,  old_dw, new_dw);
    
    scheduleAccumulateEnergySourceSinks( 
        patch,  sched,  old_dw, new_dw);
    
    scheduleComputeLagrangianValues( 
        patch,  sched,  old_dw, new_dw);
    
    scheduleAddExchangeToMomentumAndEnergy(
        patch,  sched,  old_dw, new_dw);
    
    scheduleAdvectAndAdvanceInTime(
        patch,  sched,  old_dw, new_dw);
  }
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeEquilibrationPressure--
 Purpose~   Compute the equilibration pressure
_____________________________________________________________________*/
void ICE::scheduleComputeEquilibrationPressure(
             const Patch* patch, SchedulerP& sched,
		DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::computeEquilibrationPressure",
                        patch, old_dw, new_dw,this,
			   &ICE::computeEquilibrationPressure);
  
  task->requires(old_dw,lb->press_CCLabel, 0,patch,Ghost::None);
  
  int numMatls=d_sharedState->getNumICEMatls();
  for (int m = 0; m < numMatls; m++)  {
    ICEMaterial*  matl = d_sharedState->getICEMaterial(m);
    int dwindex = matl->getDWIndex();

    task->requires(old_dw,lb->rho_CCLabel,       dwindex,patch,Ghost::None);
    task->requires(old_dw,lb->temp_CCLabel,      dwindex,patch,Ghost::None);
    task->requires(old_dw,lb->cv_CCLabel,        dwindex,patch,Ghost::None);

    task->computes(new_dw,lb->speedSound_CCLabel,dwindex, patch);
    task->computes(new_dw,lb->vol_frac_CCLabel,  dwindex, patch);
    task->computes(new_dw,lb->rho_micro_CCLabel, dwindex, patch);
  }

  task->computes(new_dw,lb->press_equil_CCLabel,0, patch);
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeFaceCenteredVelocities--
 Purpose~   Compute the face-centered velocities
_____________________________________________________________________*/
void ICE::scheduleComputeFaceCenteredVelocities(
                      const Patch* patch,SchedulerP& sched,
			 DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::computeFaceCenteredVelocities",
                        patch, old_dw, new_dw,this,
			   &ICE::computeFaceCenteredVelocities);
                        
  int numALLMatls = d_sharedState->getNumMatls();
  int dwindex;
  task->requires(new_dw,lb->press_equil_CCLabel,0,patch,Ghost::None);
  

  for (int m = 0; m < numALLMatls; m++) {
    Material* matl = d_sharedState->getMaterial( m );
    
    ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
    MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
    if(ice_matl){                    // I C E
      dwindex = ice_matl->getDWIndex();
      task->requires(old_dw,lb->rho_CCLabel,   dwindex,patch,Ghost::None);
      task->requires(old_dw,lb->vel_CCLabel,   dwindex,patch,Ghost::None);
    }
    if(mpm_matl){                    // M P M
      dwindex = mpm_matl->getDWIndex();
      task->requires(new_dw,lb->rho_CCLabel,   dwindex,patch,Ghost::None);
      task->requires(new_dw,lb->vel_CCLabel,   dwindex,patch,Ghost::None);
    } 
    task->requires(new_dw,lb->rho_micro_CCLabel,
                                             dwindex,patch,Ghost::None);

    task->computes(new_dw,lb->uvel_FCLabel,  dwindex, patch);
    task->computes(new_dw,lb->vvel_FCLabel,  dwindex, patch);
    task->computes(new_dw,lb->wvel_FCLabel,  dwindex, patch);
  }
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAddExchangeContributionToFCVel--
 Purpose~   Schedule compute the momentum exchange for the face centered 
            velocities
_____________________________________________________________________*/
void ICE::scheduleAddExchangeContributionToFCVel(
                    const Patch* patch, SchedulerP& sched,
		      DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::addExchangeContributionToFCVel",
                        patch, old_dw, new_dw,this,
			   &ICE::addExchangeContributionToFCVel);
  int numMatls=d_sharedState->getNumMatls();
  
  for (int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires(new_dw,lb->rho_micro_CCLabel,
		                                  dwindex,patch,Ghost::None);
    task->requires(new_dw,lb->vol_frac_CCLabel, dwindex, patch,Ghost::None);
    task->requires(old_dw,lb->uvel_FCLabel,     dwindex, patch,Ghost::None);
    task->requires(old_dw,lb->vvel_FCLabel,     dwindex, patch,Ghost::None);
    task->requires(old_dw,lb->wvel_FCLabel,     dwindex, patch,Ghost::None);
    
    task->computes(new_dw,lb->uvel_FCMELabel,   dwindex, patch);
    task->computes(new_dw,lb->vvel_FCMELabel,   dwindex, patch);
    task->computes(new_dw,lb->wvel_FCMELabel,   dwindex, patch);
  }
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputeDelPressAndUpdatePressCC--
 Purpose~   Schedule compute delpress and new press_CC
_____________________________________________________________________*/
void ICE::scheduleComputeDelPressAndUpdatePressCC(
                const Patch* patch,SchedulerP& sched,
		  DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::computeDelPressAndUpdatePressCC",
                        patch, old_dw, new_dw,this,
			   &ICE::computeDelPressAndUpdatePressCC);
  
  task->requires(new_dw,lb->press_equil_CCLabel, 0,patch,Ghost::None);
  int numMatls=d_sharedState->getNumMatls();
  for (int m = 0; m < numMatls; m++)  {
    Material* matl = d_sharedState->getMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires( new_dw, lb->vol_frac_CCLabel,  dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->uvel_FCMELabel,    dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->vvel_FCMELabel,    dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->wvel_FCMELabel,    dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->speedSound_CCLabel,dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->rho_micro_CCLabel, dwindex,patch,Ghost::None);
  }
  task->computes(   new_dw,lb->press_CCLabel,     0,     patch);
  task->computes(   new_dw,lb->delPress_CCLabel,  0,     patch);
  
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleComputePressFC--
 Purpose~   Schedule compute face centered pressure press_FC
_____________________________________________________________________*/
void ICE::scheduleComputePressFC(const Patch* patch, SchedulerP& sched,
			DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{                     
  Task* task = scinew Task("ICE::computePressFC",patch, old_dw, new_dw,this,
                           &ICE::computePressFC);

  task->requires(   new_dw,lb->press_CCLabel, 0,      patch,  Ghost::None);
  for (int m = 0; m < d_sharedState->getNumMatls(); m++)  {
    Material* matl = d_sharedState->getMaterial(m);
    int dwindex = matl->getDWIndex();
    ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
    MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
    if(ice_matl){
      task->requires(old_dw,lb->rho_CCLabel,   dwindex,patch,Ghost::None);
    }
    if(mpm_matl){
      task->requires(new_dw,lb->rho_CCLabel,   dwindex,patch,Ghost::None);
    }
  }

  task->computes(   new_dw, lb->pressX_FCLabel, 0,      patch);
  task->computes(   new_dw, lb->pressY_FCLabel, 0,      patch);
  task->computes(   new_dw, lb->pressZ_FCLabel, 0,      patch);

  sched->addTask(task);
}
/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAccumulateMomentumSourceSinks--
 Purpose~   Schedule compute sources and sinks of momentum
_____________________________________________________________________*/
void ICE::scheduleAccumulateMomentumSourceSinks(
            const Patch* patch, SchedulerP& sched,
	     DataWarehouseP& old_dw,DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::accumulateMomentumSourceSinks", 
                        patch, old_dw, new_dw,this,
			   &ICE::accumulateMomentumSourceSinks);

  task->requires(new_dw,    lb->pressX_FCLabel,     0,  patch,  Ghost::None);
  task->requires(new_dw,    lb->pressY_FCLabel,     0,  patch,  Ghost::None);
  task->requires(new_dw,    lb->pressZ_FCLabel,     0,  patch,  Ghost::None);
  int numMatls=d_sharedState->getNumMatls();
  
  for (int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires(old_dw,  lb->rho_CCLabel,        dwindex,patch,Ghost::None);
    task->requires(old_dw,  lb->vel_CCLabel,        dwindex,patch,Ghost::None);
    task->requires(old_dw,  lb->viscosity_CCLabel,  dwindex,patch,Ghost::None);
    task->requires(new_dw,  lb->vol_frac_CCLabel,   dwindex,patch,Ghost::None);
    task->computes(new_dw,  lb->mom_source_CCLabel, dwindex,patch);
    task->computes(new_dw,  lb->tau_X_FCLabel,      dwindex,patch);
    task->computes(new_dw,  lb->tau_Y_FCLabel,      dwindex,patch);
    task->computes(new_dw,  lb->tau_Z_FCLabel,      dwindex,patch);
  }
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAccumulateEnergySourceSinks--
 Purpose~   Schedule compute sources and sinks of energy
_____________________________________________________________________*/
void ICE::scheduleAccumulateEnergySourceSinks(
            const Patch* patch,SchedulerP& sched,
	     DataWarehouseP& old_dw,  DataWarehouseP& new_dw)

{
  Task* task = scinew Task("ICE::accumulateEnergySourceSinks",
                        patch, old_dw, new_dw,this,
			   &ICE::accumulateEnergySourceSinks);
  
  task->requires(new_dw,    lb->press_CCLabel,    0, patch, Ghost::None);
  task->requires(new_dw,    lb->delPress_CCLabel, 0, patch, Ghost::None);
  int numMatls=d_sharedState->getNumMatls();
  
  for (int m = 0; m < numMatls; m++)  {
    Material* matl = d_sharedState->getMaterial(m);
    int dwindex = matl->getDWIndex();      
    task->requires( new_dw, lb->rho_micro_CCLabel, dwindex, patch,Ghost::None);
    task->requires( new_dw, lb->speedSound_CCLabel,dwindex, patch,Ghost::None);
    task->requires( new_dw, lb->vol_frac_CCLabel,  dwindex, patch,Ghost::None);
    task->computes (new_dw, lb->int_eng_source_CCLabel, dwindex, patch);
  }
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE:: scheduleComputeLagrangianValues--
 Purpose~   Schedule compute lagrangian mass momentum and internal energy
 Note:      Only loop over ICE materials, mom_L for MPM is computed
            prior to this function.  
_____________________________________________________________________*/
void ICE:: scheduleComputeLagrangianValues(
            const Patch* patch, SchedulerP&  sched,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::computeLagrangianValues",
                            patch, old_dw, new_dw,this,
			       &ICE::computeLagrangianValues);
  int numMatls=d_sharedState->getNumICEMatls();
  for (int m = 0; m < numMatls; m++)   {
    ICEMaterial* matl = d_sharedState->getICEMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires( old_dw, lb->rho_CCLabel,        dwindex,patch,Ghost::None);
    task->requires( old_dw, lb->vel_CCLabel,        dwindex,patch,Ghost::None);
    task->requires( old_dw, lb->cv_CCLabel,         dwindex,patch,Ghost::None);
    task->requires( old_dw, lb->temp_CCLabel,       dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->mom_source_CCLabel, dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->int_eng_source_CCLabel,
		                                      dwindex,patch,Ghost::None);
    
    task->computes( new_dw, lb->mom_L_CCLabel,      dwindex,patch);
    task->computes( new_dw, lb->int_eng_L_CCLabel,  dwindex,patch);
    task->computes( new_dw, lb->mass_L_CCLabel,     dwindex,patch);
  }
  // Use this data for comparing and contrasting with SA ICE
  for (int m = 0; m < d_sharedState->getNumMPMMatls(); m++)  {
    MPMMaterial* matl = d_sharedState->getMPMMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires( new_dw, MIlb->mom_L_CCLabel,    dwindex,patch,Ghost::None);
    task->requires( new_dw, MIlb->int_eng_L_CCLabel,dwindex,patch,Ghost::None);
 } 
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAddExchangeToMomentumAndEnergy--
 Purpose~   Schedule momentum and energy exchange on the lagrangian quantities
_____________________________________________________________________*/
void ICE::scheduleAddExchangeToMomentumAndEnergy(
            const Patch* patch,SchedulerP& sched,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::addExchangeToMomentumAndEnergy",
                        patch, old_dw, new_dw,this,
			   &ICE::addExchangeToMomentumAndEnergy);
  int numMatls=d_sharedState->getNumICEMatls();
  
  for (int m = 0; m < numMatls; m++)  {
    ICEMaterial* matl = d_sharedState->getICEMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires( old_dw, lb->rho_CCLabel,        dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->mom_L_CCLabel,      dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->int_eng_L_CCLabel,  dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->vol_frac_CCLabel,   dwindex,patch,Ghost::None);
    task->requires( old_dw, lb->cv_CCLabel,         dwindex,patch,Ghost::None);
    task->requires( new_dw, lb->rho_micro_CCLabel,  dwindex,patch,Ghost::None);
    
    task->computes( new_dw, lb->mom_L_ME_CCLabel,    dwindex,patch);
    task->computes( new_dw, lb->int_eng_L_ME_CCLabel,dwindex,patch);
  }
  sched->addTask(task);
}

/* ---------------------------------------------------------------------
 Function~  ICE::scheduleAdvectAndAdvanceInTime--
 Purpose~   Schedule advance and advect in time for mass, momentum
            and energy.  Note this function puts (*)vel_CC, rho_CC
            and Temp_CC into new dw, not flux variables
_____________________________________________________________________*/
void ICE::scheduleAdvectAndAdvanceInTime(
            const Patch* patch, SchedulerP& sched,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  Task* task = scinew Task("ICE::advectAndAdvanceInTime",
                        patch, old_dw, new_dw,this,
			   &ICE::advectAndAdvanceInTime);
  int numMatls=d_sharedState->getNumICEMatls();
  for (int m = 0; m < numMatls; m++ )   {
    ICEMaterial* matl = d_sharedState->getICEMaterial(m);
    int dwindex = matl->getDWIndex();
    task->requires(old_dw, lb->cv_CCLabel,dwindex,patch,Ghost::None,0);
    task->requires(old_dw, lb->rho_CCLabel,       dwindex,patch,Ghost::None);
    task->requires(old_dw, lb->vel_CCLabel,       dwindex,patch,Ghost::None);
    task->requires(old_dw, lb->temp_CCLabel,      dwindex,patch,Ghost::None);
    task->requires(new_dw, lb->mom_L_ME_CCLabel,  dwindex,patch,Ghost::None,0);
    task->requires(new_dw, lb->int_eng_L_ME_CCLabel,dwindex,patch,
		   Ghost::None,0);    
    task->computes(new_dw, lb->temp_CCLabel,      dwindex,patch);
    task->computes(new_dw, lb->rho_CCLabel,       dwindex,patch);
    task->computes(new_dw, lb->cv_CCLabel,        dwindex,patch);
    task->computes(new_dw, lb->vel_CCLabel,       dwindex,patch);
  }
  sched->addTask(task);
}


/* ---------------------------------------------------------------------
 Function~  ICE::actuallyComputeStableTimestep--
 Purpose~   Compute next time step based on speed of sound and 
            maximum velocity in the domain
_____________________________________________________________________*/
void ICE::actuallyComputeStableTimestep(
    const ProcessorGroup*,
    const Patch*    patch,
    DataWarehouseP& old_dw,
    DataWarehouseP& new_dw)
{
  cout << "Doing Compute Stable Timestep \t\t\t ICE" << endl;
  double dT = d_initialDt;

  if (computeDt) {
    Vector dx = patch->dCell();
    double delt_CFL = 100000, fudge_factor = 1.;
    CCVariable<double> speedSound;
    CCVariable<Vector> vel;
    double CFL,N_ITERATIONS_TO_STABILIZE = 1;
    
    ::iterNum++;
    if (iterNum < N_ITERATIONS_TO_STABILIZE) {
      CFL = d_CFL * (double)(::iterNum)*(1./(double)N_ITERATIONS_TO_STABILIZE);
    } else {
      CFL = d_CFL;
    }
    
    for (int m = 0; m < d_sharedState->getNumICEMatls(); m++) {
      ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
      int dwindex = ice_matl->getDWIndex();
     
      new_dw->get(speedSound, lb->speedSound_CCLabel,
		                          dwindex,patch,Ghost::None, 0);
      new_dw->get(vel, lb->vel_CCLabel, dwindex,patch,Ghost::None, 0);
      
      for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){   
	double A = fudge_factor*CFL*dx.x()/(speedSound[*iter] + 
					    fabs(vel[*iter].x())+ d_SMALL_NUM);
	double B = fudge_factor*CFL*dx.y()/(speedSound[*iter] + 
					    fabs(vel[*iter].y())+ d_SMALL_NUM);
	double C = fudge_factor*CFL*dx.z()/(speedSound[*iter] + 
					    fabs(vel[*iter].z())+ d_SMALL_NUM);
	
	delt_CFL = std::min(A, delt_CFL);
	delt_CFL = std::min(B, delt_CFL);
	delt_CFL = std::min(C, delt_CFL);
	
      }
    }
    dT = delt_CFL;
  }
  cout << "new dT is " << dT << endl;
  new_dw->put(delt_vartype(dT), lb->delTLabel);
  computeDt = true;

}

/* --------------------------------------------------------------------- 
 Function~  ICE::actuallyInitialize--
 Purpose~  Initialize the CC and FC variables and the pressure  
_____________________________________________________________________*/ 
void ICE::actuallyInitialize(const ProcessorGroup*, const Patch* patch,
			     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing Initialize \t\t\t ICE" << endl;
  int numMatls = d_sharedState->getNumICEMatls();
  int numALLMatls = d_sharedState->getNumMatls();
  CCVariable<double>    press_CC;  
  new_dw->allocate(press_CC,lb->press_CCLabel, 0,patch);
  
//__________________________________
// Note:
// The press_CC isn't material dependent even though
// we loop over numMatls below. This is done so we don't need additional
// machinery to grab the pressure inside a geom_object
  for (int m = 0; m < numMatls; m++ ) {
    ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
    int dwindex = ice_matl->getDWIndex();
    CCVariable<double> rho_micro,rho_CC,Temp_CC,cv,speedSound,visc_CC;
    CCVariable<double> vol_frac_CC;
    CCVariable<Vector> vel_CC;
    SFCXVariable<double> uvel_FC;
    SFCYVariable<double> vvel_FC;
    SFCZVariable<double> wvel_FC;
    
    new_dw->allocate(rho_micro,   lb->rho_micro_CCLabel,  dwindex,patch);
    new_dw->allocate(rho_CC,      lb->rho_CCLabel,        dwindex,patch);
    new_dw->allocate(Temp_CC,     lb->temp_CCLabel,       dwindex,patch);
    new_dw->allocate(cv,          lb->cv_CCLabel,         dwindex,patch);
    new_dw->allocate(speedSound,  lb->speedSound_CCLabel, dwindex,patch);
    new_dw->allocate(visc_CC,     lb->viscosity_CCLabel,  dwindex,patch);
    new_dw->allocate(vol_frac_CC, lb->vol_frac_CCLabel,   dwindex,patch);
    new_dw->allocate(vel_CC,      lb->vel_CCLabel,        dwindex,patch);
    
    new_dw->allocate(uvel_FC,     lb->uvel_FCLabel,       dwindex,patch);
    new_dw->allocate(vvel_FC,     lb->vvel_FCLabel,       dwindex,patch);
    new_dw->allocate(wvel_FC,     lb->wvel_FCLabel,       dwindex,patch);
    
    ice_matl->initializeCells(rho_micro,rho_CC,Temp_CC,cv,speedSound,visc_CC,
		        vol_frac_CC,vel_CC,press_CC,numALLMatls,patch,new_dw);

    uvel_FC.initialize(0.);
    vvel_FC.initialize(0.);
    wvel_FC.initialize(0.);
    
    setBC(rho_CC, "Density",      patch);
    setBC(Temp_CC,"Temperature",  patch);
    setBC(vel_CC, "Velocity",     patch);

    new_dw->put(rho_micro,  lb->rho_micro_CCLabel, dwindex,patch);
    new_dw->put(rho_CC,     lb->rho_CCLabel,       dwindex,patch);
    new_dw->put(vol_frac_CC,lb->vol_frac_CCLabel,  dwindex,patch);
    new_dw->put(Temp_CC,    lb->temp_CCLabel,      dwindex,patch);
    new_dw->put(cv,         lb->cv_CCLabel,        dwindex,patch);
    new_dw->put(speedSound, lb->speedSound_CCLabel,dwindex,patch);
    new_dw->put(vel_CC,     lb->vel_CCLabel,       dwindex,patch);
    new_dw->put(uvel_FC,    lb->uvel_FCLabel,      dwindex,patch);
    new_dw->put(vvel_FC,    lb->vvel_FCLabel,      dwindex,patch);
    new_dw->put(wvel_FC,    lb->wvel_FCLabel,      dwindex,patch);
    new_dw->put(visc_CC,    lb->viscosity_CCLabel, dwindex,patch);
  
  
/*`==========TESTING==========*/ 
if (switchDebugInitialize){
  cout << " Initial Conditions" << endl;
  
  IntVector lowIndex     = patch->getInteriorCellLowIndex();
  IntVector highIndex    = patch->getInteriorCellHighIndex();
  cout << "\n\t xLoLimit   = "<<lowIndex.x()<< 
    "\t yLoLimit   is "<<lowIndex.y()<<
    "\t zLoLimit   is "<<lowIndex.z()<< endl;
  cout << "\t xHiLimit   is "<<highIndex.x()<< 
    "\t yHiLimit   is "<<highIndex.y()<<
    "\t zHiLimit   is "<<highIndex.z()<< endl;
  
  IntVector loIndex   = patch->getCellLowIndex();
  IntVector hiIndex   = patch->getCellHighIndex();
  cout << "\n\txLo_GC   is "<<loIndex.x()<< 
    "\t yLo_GC   is "<<loIndex.y()<<
    "\t zLo_GC   is "<<loIndex.z()<< endl;
  cout << "\t xHi_GC   is "<<hiIndex.x()<< 
    "\t yHi_GC   is "<<hiIndex.y()<<
    "\t zHi_GC   is "<<hiIndex.z()<< endl;
  
  Vector dx = patch->dCell();
  cout << "\n\tdx     is "<< dx.x() << 
    "\tdy     is "<< dx.y() << 
    "\tdz     is"<< dx.z() << endl;
       
  char description[50];
  sprintf(description, "Initialization_Mat_%d ",dwindex);
  printData(   patch, 1, description, "rho_CC",         rho_CC);
  printData(   patch, 1, description, "rho_micro_CC",   rho_micro);
  printData(   patch, 1, description, "Temp_CC",        Temp_CC);
  printData(   patch, 1, description, "vol_frac_CC",    vol_frac_CC);
  printVector( patch, 1, description, "uvel_CC", 0,  vel_CC);
  printVector( patch, 1, description, "vvel_CC", 1,  vel_CC);
  printVector( patch, 1, description, "wvel_CC", 2,  vel_CC);
} 
 /*==========TESTING==========`*/   
  }
  setBC(press_CC,"Pressure",patch);
  if (switchDebugInitialize){
     printData(   patch, 1, "Initialization", "press_CC", press_CC);
  }
  new_dw->put(press_CC,    lb->press_CCLabel,  0,patch);
}

/* --------------------------------------------------------------------- 
 Function~  ICE::computeEquilibrationPressure--
 Purpose~   Find the equilibration pressure  
 Reference: Flow of Interpenetrating Material Phases, J. Comp, Phys
               18, 440-464, 1975, see the equilibration section
                   
 Steps
 ----------------
    - Compute rho_micro_CC, SpeedSound, vol_frac

    For each cell
    _ WHILE LOOP(convergence, max_iterations)
        - compute the pressure and dp_drho from the EOS of each material.
        - Compute delta Pressure
        - Compute delta volume fraction and update the 
          volume fraction and the celldensity.
        - Test for convergence of delta pressure and delta volume fraction
    - END WHILE LOOP
    - bulletproofing
    end
 
Note:  The nomenclature follows the reference.                                 
_____________________________________________________________________*/
void ICE::computeEquilibrationPressure(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  double    converg_coeff = 10;              
  double    convergence_crit = converg_coeff * DBL_EPSILON;
  double    sum, tmp;

  int numMatls = d_sharedState->getNumICEMatls();
  vector<CCVariable<double> > vol_frac(numMatls);
  char warning[100];
  static int n_passes;                  
  n_passes ++; 
  cout << "Doing calc_equilibration_pressure \t\t ICE" << endl;
 
  vector<double> delVol_frac(numMatls),press_eos(numMatls);
  vector<double> dp_drho(numMatls),dp_de(numMatls);
  
  vector<CCVariable<double> > rho_micro(numMatls);
  vector<CCVariable<double> > rho_CC(numMatls);
  vector<CCVariable<double> > cv(numMatls);
  vector<CCVariable<double> > Temp(numMatls);
  vector<CCVariable<double> > speedSound(numMatls),speedSound_new(numMatls);
  CCVariable<double> press,press_new;
  
  old_dw->get(press,         lb->press_CCLabel, 0,patch,Ghost::None, 0); 
  new_dw->allocate(press_new,lb->press_CCLabel, 0,patch);

  for (int m = 0; m < numMatls; m++) {
    ICEMaterial* matl = d_sharedState->getICEMaterial(m);
    int dwindex = matl->getDWIndex();
    old_dw->get(cv[m],        lb->cv_CCLabel,  dwindex, patch, Ghost::None, 0);
    old_dw->get(rho_CC[m],    lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
    old_dw->get(Temp[m],      lb->temp_CCLabel,dwindex, patch, Ghost::None, 0);              
    new_dw->allocate(speedSound_new[m],lb->speedSound_CCLabel,dwindex, patch);
    new_dw->allocate(rho_micro[m],     lb->rho_micro_CCLabel, dwindex, patch);
    new_dw->allocate( vol_frac[m],     lb->vol_frac_CCLabel,  dwindex, patch); 
  }
  press_new = press;
  //__________________________________
  // Compute rho_micro, speedSound, and volfrac
  for (CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++) {
       for (int m = 0; m < numMatls; m++) {
       ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
       double gamma = ice_matl->getGamma();
                     
        rho_micro[m][*iter] =  ice_matl->getEOS()->computeRhoMicro(
                                            press_new[*iter],gamma,
					         cv[m][*iter],Temp[m][*iter]); 
                                                                     
        ice_matl->getEOS()->computePressEOS(rho_micro[m][*iter],gamma,
                                            cv[m][*iter],Temp[m][*iter],
                                            press_eos[m],dp_drho[m], dp_de[m]);
                                            
        tmp = dp_drho[m] + dp_de[m] * 
                    (press_eos[m]/(rho_micro[m][*iter]*rho_micro[m][*iter]));
        speedSound_new[m][*iter] = sqrt(tmp);
        
        vol_frac[m][*iter] = rho_CC[m][*iter]/rho_micro[m][*iter];
     }
 }

/*`==========DEBUG============*/ 
  if (switchDebug_equilibration_press) {
    printData( patch, 1, "TOP_equilibration", "Press_CC_top", press);
              
   for (int m = 0; m < numMatls; m++)  {
     ICEMaterial* matl = d_sharedState->getICEMaterial( m );
     int dwindex = matl->getDWIndex(); 
     char description[50];
     sprintf(description, "TOP_equilibration_Mat_%d ",dwindex);
     printData( patch, 1, description, "rho_CC",          rho_CC[m]);
     printData( patch, 0, description, "speedSound",      speedSound_new[m]);
     printData( patch, 1, description, "Temp_CC",         Temp[m]);
    }
  }
 /*==========DEBUG============`*/
   
//______________________________________________________________________
// Done with preliminary calcs, now loop over every cell
  int count, test_max_iter = 0;
  for (CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++) {
    
    IntVector curcell = *iter;    //So I have a chance at finding the bug
    int i,j,k;
    i   = curcell.x();
    j   = curcell.y();
    k   = curcell.z();
      
    double delPress = 0.;
    bool converged  = false;
    count           = 0;
    while ( count < d_max_iter_equilibration && converged == false) {
      count++;
      double A = 0.;
      double B = 0.;
      double C = 0.;
      
      for (int m = 0; m < numMatls; m++) 
        delVol_frac[m] = 0.;
      //__________________________________
     // evaluate press_eos at cell i,j,k
     for (int m = 0; m < numMatls; m++)  {
       ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
       double gamma = ice_matl->getGamma();
       
       ice_matl->getEOS()->computePressEOS(rho_micro[m][*iter],gamma,
                                           cv[m][*iter],Temp[m][*iter],
                                           press_eos[m], dp_drho[m], dp_de[m]);
     }
     //__________________________________
     // - compute delPress
     // - update press_CC     
     vector<double> Q(numMatls),y(numMatls);     
     for (int m = 0; m < numMatls; m++)   {
       Q[m] =  press_new[*iter] - press_eos[m];
       y[m] =  dp_drho[m] * ( rho_CC[m][*iter]/
               (vol_frac[m][*iter] * vol_frac[m][*iter] + d_SMALL_NUM) ); 
       A   +=  vol_frac[m][*iter];
       B   +=  Q[m]/(y[m] + d_SMALL_NUM);
       C   +=  1.0/(y[m]  + d_SMALL_NUM);
     }
     double vol_frac_not_close_packed = 1.;
     delPress = (A - vol_frac_not_close_packed - B)/C;
     
     press_new[*iter] += delPress;
     
     //__________________________________
     // backout rho_micro_CC at this new pressure
     for (int m = 0; m < numMatls; m++) {
       ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
       double gamma = ice_matl->getGamma();
       
       rho_micro[m][*iter] = 
         ice_matl->getEOS()->computeRhoMicro(press_new[*iter],gamma,
                                             cv[m][*iter],Temp[m][*iter]);
     }
     //__________________________________
     // - compute the updated volume fractions
     //  There are two different way of doing it
     for (int m = 0; m < numMatls; m++)  {
       delVol_frac[m]       = -(Q[m] + delPress)/( y[m] + d_SMALL_NUM );
     //vol_frac[m][*iter]   += delVol_frac[m];
       vol_frac[m][*iter]   = rho_CC[m][*iter]/rho_micro[m][*iter];
     }
     //__________________________________
     // Find the speed of sound at ijk
     // needed by eos and the the explicit
     // del pressure function
     for (int m = 0; m < numMatls; m++)  {
        ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
        double gamma = ice_matl->getGamma();
        ice_matl->getEOS()->computePressEOS(rho_micro[m][*iter],gamma,
                                            cv[m][*iter],Temp[m][*iter],
                                            press_eos[m],dp_drho[m], dp_de[m]);
         
        tmp = dp_drho[m] + dp_de[m] * 
                    (press_eos[m]/(rho_micro[m][*iter]*rho_micro[m][*iter]));
        speedSound_new[m][*iter] = sqrt(tmp);
     }
     //__________________________________
     // - Test for convergence 
     //  If sum of vol_frac_CC ~= 1.0 then converged 
     sum = 0.0;
     for (int m = 0; m < numMatls; m++)  {
       sum += vol_frac[m][*iter];
     }
     if (fabs(sum-1.0) < convergence_crit)
       converged = true;
     
    }   // end of converged
    
    test_max_iter = std::max(test_max_iter, count);
    
    //__________________________________
    //      BULLET PROOFING
    if(test_max_iter == d_max_iter_equilibration)
    {
        sprintf(warning, 
        " cell[%d][%d][%d], iter %d, n_passes %d,Now exiting ",
        i,j,k,count,n_passes);
         Message(1,"calc_equilibration_press:",
            " Maximum number of iterations was reached ", warning);
    }
    
     for (int m = 0; m < numMatls; m++) {
         ASSERT(( vol_frac[m][*iter] > 0.0 ) ||
                ( vol_frac[m][*iter] < 1.0));
     }
    if ( fabs(sum - 1.0) > convergence_crit)   {
        sprintf(warning, 
        " cell[%d][%d][%d], iter %d, n_passes %d,Now exiting ",
        i,j,k,count,n_passes);
        Message(1,"calc_equilibration_press:",
            " sum(vol_frac_CC) != 1.0", warning);
    }

    if ( press_new[*iter] < 0.0 )   {
        sprintf(warning, 
        " cell[%d][%d][%d], iter %d, n_passes %d, Now exiting",
         i,j,k, count, n_passes);
        Message(1,"calc_equilibration_press:", 
            " press_new[iter*] < 0", warning);
    }

    for (int m = 0; m < numMatls; m++)
    if ( rho_micro[m][*iter] < 0.0 || vol_frac[m][*iter] < 0.0) {
        sprintf(warning, 
        " cell[%d][%d][%d], mat %d, iter %d, n_passes %d,Now exiting ",
        i,j,k,m,count,n_passes);
        Message(1," calc_equilibration_press:", 
            " rho_micro < 0 || vol_frac < 0", warning);
    }
  }     // end of cell interator

    fprintf(stderr, "\n max number of iterations in any cell %i\n",test_max_iter);
  /*__________________________________
   *   THIS NEEDS TO BE FIXED 
   *   WE NEED TO UPDATE BC_VALUES NOT PRESSURE
   *   SINCE WE USE BC_VALUES LATER ON IN THE CODE
   *___________________________________*/
  setBC(press_new,"Pressure",patch);
  
#if 0
  
  // Hydrostatic pressure adjustment - subtract off the hydrostatic pressure
  
  Vector dx             = patch->dCell();
  Vector gravity        = d_sharedState->getGravity();
  IntVector highIndex   = patch->getCellHighIndex();
  IntVector lowIndex    = patch->getCellLowIndex();
  
  double width  = (highIndex.x() - lowIndex.x())*dx.x();
  double height = (highIndex.y() - lowIndex.y())*dx.y();
  double depth  = (highIndex.z() - lowIndex.z())*dx.z();
  
  if (gravity.x() != 0.)  {
    // x direction
    for (CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
        iter++) {
      IntVector curcell = *iter;
      double press_hydro = 0.;
      for (int m = 0; m < numMatls; m++)  {
        press_hydro += rho[m][*iter]* gravity.x()*
          ((double) (curcell-highIndex).x()*dx.x()- width);
      }
      press_new[*iter] -= press_hydro;
    }
  }
  if (gravity.y() != 0.)  {
    // y direction
    for (CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
        iter++)   {
      IntVector curcell = *iter;
      double press_hydro = 0.;
      for (int m = 0; m < numMatls; m++)  {
        press_hydro += rho[m][*iter]* gravity.y()*
          ( (double) (curcell-highIndex).y()*dx.y()- height);
      }
      press_new[*iter] -= press_hydro;
    }
  }
  if (gravity.z() != 0.)  {
    // z direction
    for (CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
        iter++)   {
      IntVector curcell = *iter;
      double press_hydro = 0.;
      for (int m = 0; m < numMatls; m++)  {
        press_hydro += rho[m][*iter]* gravity.z()*
          ((double) (curcell-highIndex).z()*dx.z()- depth);
      }
      press_new[*iter] -= press_hydro;
    }
  }   
#endif
  
  for (int m = 0; m < numMatls; m++)   {
    ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
    int dwindex = ice_matl->getDWIndex();
    new_dw->put( vol_frac[m],      lb->vol_frac_CCLabel,   dwindex, patch);
    new_dw->put( speedSound_new[m],lb->speedSound_CCLabel, dwindex, patch);
    new_dw->put( rho_micro[m],     lb->rho_micro_CCLabel,  dwindex, patch);
  }
  new_dw->put(press_new,lb->press_equil_CCLabel,0,patch);
  
  
/*`==========DEBUG============*/ 
  if (switchDebug_equilibration_press) {
   printData( patch, 1, "BOTTOM", "Press_CC_equil", press_new);
              
   for (int m = 0; m < numMatls; m++)  {
     ICEMaterial* matl = d_sharedState->getICEMaterial( m );
     int dwindex = matl->getDWIndex(); 
     char description[50];
     sprintf(description, "BOT_equilibration_Mat_%d ",dwindex);
     printData( patch, 1, description, "rho_CC",          rho_CC[m]);
     printData( patch, 1, description, "speedSound",      speedSound_new[m]);
     printData( patch, 1, description, "Temp_CC",         Temp[m]);
    }
  }
 /*==========DEBUG============`*/
}

/* ---------------------------------------------------------------------
 Function~  ICE::computeFaceCenteredVelocities--
 Purpose~   compute the face centered velocities minus the exchange
            contribution.
_____________________________________________________________________*/
void ICE::computeFaceCenteredVelocities(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing compute_face_centered_velocities \t\t ICE" << endl;

  int numMatls = d_sharedState->getNumMatls();

  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());
  Vector dx      = patch->dCell();
  Vector gravity = d_sharedState->getGravity();

  CCVariable<double> rho_CC, rho_micro_CC;
  CCVariable<Vector> vel_CC;
  CCVariable<double> press_CC;
  new_dw->get(press_CC,lb->press_equil_CCLabel, 0, patch, Ghost::None, 0);
  
  
  // Compute the face centered velocities
  for(int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();

    ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
    MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
    if(ice_matl){
      int dwindex = ice_matl->getDWIndex();
      old_dw->get(rho_CC, lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
      old_dw->get(vel_CC, lb->vel_CCLabel, dwindex, patch, Ghost::None, 0);
      new_dw->get(rho_micro_CC, lb->rho_micro_CCLabel,dwindex,patch,
                                                           Ghost::None, 0);
    }
    if(mpm_matl){
      int dwindex = mpm_matl->getDWIndex();
      new_dw->get(rho_CC, lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
      new_dw->get(vel_CC, lb->vel_CCLabel, dwindex, patch, Ghost::None, 0);
      new_dw->get(rho_micro_CC, lb->rho_micro_CCLabel,dwindex,patch,
                                                           Ghost::None, 0);
    }

 
#if 0
/*`==========TESTING==========*/ 
    if (switchDebug_vel_FC ) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex(); 
    char description[50];
    sprintf(description, "TOP_vel_FC_Mat_%d ",dwindex); 
    printData( patch, 1, description, "rho_CC",      rho_CC);
    printData( patch, 1, description, "rho_micro_CC",rho_micro_CC);
    printVector( patch,1, description, "uvel_CC", 0, vel_CC);
    }
 /*==========TESTING==========`*/
#endif    
    
    SFCXVariable<double> uvel_FC;
    SFCYVariable<double> vvel_FC;
    SFCZVariable<double> wvel_FC;
    new_dw->allocate(uvel_FC, lb->uvel_FCLabel, dwindex, patch);
    new_dw->allocate(vvel_FC, lb->vvel_FCLabel, dwindex, patch);
    new_dw->allocate(wvel_FC, lb->wvel_FCLabel, dwindex, patch);
    
    uvel_FC.initialize(0.);
    vvel_FC.initialize(0.);
    wvel_FC.initialize(0.);
    
    double term1, term2, term3, press_coeff, rho_micro_FC, rho_FC;

    for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
	  iter++)   {
      IntVector curcell = *iter;
      
      //__________________________________
      //    T O P   F A C E S 
      //   Extend the computations into the left
      //   and right ghost cells 
      if (curcell.y() < (patch->getCellHighIndex()).y()-1)      {
	IntVector adjcell(curcell.x(),curcell.y()+1,curcell.z()); 
	
	rho_micro_FC = rho_micro_CC[adjcell] + rho_micro_CC[curcell];
	rho_FC       = rho_CC[adjcell]       + rho_CC[curcell];
	//__________________________________
	// interpolation to the face
	term1 = (rho_CC[adjcell] * vel_CC[adjcell].y() +
		 rho_CC[curcell] * vel_CC[curcell].y())/(rho_FC);            
	//__________________________________
	// pressure term
	press_coeff = 2.0/(rho_micro_FC);
	term2 =   delT * press_coeff *
	  (press_CC[adjcell] - press_CC[curcell])/dx.y();                
	//__________________________________
	// gravity term
	term3 =  delT * gravity.y();
	vvel_FC[curcell + IntVector(0,1,0)] = term1- term2 + term3;
      }
      
      //__________________________________
      //  R I G H T   F A C E 
      // Extend the computations to the 
      // top and bottom ghostcells 
      if (curcell.x() < (patch->getCellHighIndex()).x()-1) {
	IntVector adjcell(curcell.x()+1,curcell.y(),curcell.z()); 
	
	rho_micro_FC = rho_micro_CC[adjcell] + rho_micro_CC[curcell];
	rho_FC       = rho_CC[adjcell]       + rho_CC[curcell];
	//__________________________________
	// interpolation to the face
	term1 = (rho_CC[adjcell] * vel_CC[adjcell].x() +
		  rho_CC[curcell] * vel_CC[curcell].x())/(rho_FC);
	//__________________________________
	// pressure term
	press_coeff = 2.0/(rho_micro_FC);
	
	term2 =   delT * press_coeff *
	  (press_CC[adjcell] - press_CC[curcell])/dx.x();
	//__________________________________
	// gravity term
	term3 =  delT * gravity.x();
	
	uvel_FC[curcell + IntVector(1,0,0)] = term1- term2 + term3;
      }

      //__________________________________
      //  F R O N T   F A C E
      // Extend the computations to the front
      // and back ghostcells
      if (curcell.z() < (patch->getCellHighIndex()).z()-1)  {
	IntVector adjcell(curcell.x(),curcell.y(),curcell.z()+1); 
	
	rho_micro_FC = rho_micro_CC[adjcell] + rho_micro_CC[curcell];
	rho_FC       = rho_CC[adjcell]       + rho_CC[curcell];
	
	//__________________________________
	// interpolation to the face
	term1 = (rho_CC[adjcell] * vel_CC[adjcell].z() +
		 rho_CC[curcell] * vel_CC[curcell].z())/(rho_FC);
	
	//__________________________________
	// pressure term
	press_coeff = 2.0/(rho_micro_FC);
	
	term2 =   delT * press_coeff *
	  (press_CC[adjcell] - press_CC[curcell])/dx.z();
	
	//__________________________________
	// gravity term
	term3 =  delT * gravity.z();
	
	wvel_FC[curcell + IntVector(0,0,1)] = term1- term2 + term3;
      }
    }

#if 0
/*`==========DEBUG============*/ 
    if (switchDebug_vel_FC ) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex(); 
    char description[50];
    sprintf(description, "bottom_of_vel_FC_Before_SetBC_Mat_%d ",dwindex);
    printData_FC( patch,1, description, "uvel_FC", uvel_FC);
    }
 /*==========DEBUG============`*/
 #endif    
    setBC(uvel_FC,"Velocity","x",patch);
    setBC(vvel_FC,"Velocity","y",patch);
    setBC(wvel_FC,"Velocity","z",patch);
   
    new_dw->put(uvel_FC, lb->uvel_FCLabel, dwindex, patch);
    new_dw->put(vvel_FC, lb->vvel_FCLabel, dwindex, patch);
    new_dw->put(wvel_FC, lb->wvel_FCLabel, dwindex, patch);
/*`==========DEBUG============*/ 
    if (switchDebug_vel_FC ) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex(); 
    char description[50];
    sprintf(description, "bottom_of_vel_FC_Mat_%d ",dwindex);
    printData_FC( patch,1, description, "uvel_FC", uvel_FC);
    printData_FC( patch,1, description, "vvel_FC", vvel_FC);
    printData_FC( patch,1, description, "wvel_FC", wvel_FC);
    }
 /*==========DEBUG============`*/
  }
}

/*---------------------------------------------------------------------
 Function~  addExchangeContributionToFCVel--
 Purpose~
   This function adds the momentum exchange contribution to the 
   existing face-centered velocities

 Prerequisites:
            The face centered velocity for each material without
            the exchange must be solved prior to this routine.
            
                   (A)                              (X)
| (1+b12 + b13)     -b12          -b23          |   |del_FC[1]  |    
|                                               |   |           |    
| -b21              (1+b21 + b23) -b32          |   |del_FC[2]  |    
|                                               |   |           | 
| -b31              -b32          (1+b31 + b32) |   |del_FC[2]  |

                        =
                        
                        (B)
| b12( uvel_FC[2] - uvel_FC[1] ) + b13 ( uvel_FC[3] -uvel_FC[1])    | 
|                                                                   |
| b21( uvel_FC[1] - uvel_FC[2] ) + b23 ( uvel_FC[3] -uvel_FC[2])    | 
|                                                                   |
| b31( uvel_FC[1] - uvel_FC[3] ) + b32 ( uvel_FC[2] -uvel_FC[3])    |           
 
 Steps for each face:
    1) Comute the beta coefficients
    2) Form and A matrix and B vector
    3) Solve for del_FC[*]
    4) Add del_FC[*] to the appropriate velocity
 
 References: see "A Cell-Centered ICE method for multiphase flow simulations"
 by Kashiwa, above equation 4.13.
 ---------------------------------------------------------------------  */
void ICE::addExchangeContributionToFCVel(
            const ProcessorGroup*,  const Patch* patch,
            DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing Add_exchange_contribution_to_FC_vel \t ICE" << endl;
  
  int numMatls = d_sharedState->getNumMatls();
  int itworked;
  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());
  double tmp;

  vector<CCVariable<double> > rho_micro_CC(numMatls);
  vector<CCVariable<double> > vol_frac_CC(numMatls);
  vector<SFCXVariable<double> > uvel_FC(numMatls);
  vector<SFCYVariable<double> > vvel_FC(numMatls);
  vector<SFCZVariable<double> > wvel_FC(numMatls);

  vector<SFCXVariable<double> > uvel_FCME(numMatls);
  vector<SFCYVariable<double> > vvel_FCME(numMatls);
  vector<SFCZVariable<double> > wvel_FCME(numMatls);

  // Extract the momentum exchange coefficients
  vector<double> b(numMatls);
  DenseMatrix beta(numMatls,numMatls),a(numMatls,numMatls),
    K(numMatls,numMatls);
  beta.zero();
  a.zero();
  K.zero();

  for (int i = 0; i < numMatls; i++ )  {
    K[numMatls-1-i][i] = d_K_mom[i];
  }
  
  for(int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();
    new_dw->get(rho_micro_CC[m], lb->rho_micro_CCLabel, dwindex, patch, 
		                                                Ghost::None, 0);         
    new_dw->get(vol_frac_CC[m],  lb->vol_frac_CCLabel,dwindex, patch, 
		                                                Ghost::None, 0);
    new_dw->get(uvel_FC[m], lb->uvel_FCLabel, dwindex, patch, Ghost::None, 0);
    new_dw->get(vvel_FC[m], lb->vvel_FCLabel, dwindex, patch, Ghost::None, 0);
    new_dw->get(wvel_FC[m], lb->wvel_FCLabel, dwindex, patch, Ghost::None, 0);

    new_dw->allocate(uvel_FCME[m], lb->uvel_FCMELabel, dwindex, patch);
    new_dw->allocate(vvel_FCME[m], lb->vvel_FCMELabel, dwindex, patch);
    new_dw->allocate(wvel_FCME[m], lb->wvel_FCMELabel, dwindex, patch);
  }
  
  for (int m = 0; m < numMatls; m++)  {
    uvel_FCME[m] = uvel_FC[m];
    vvel_FCME[m] = vvel_FC[m];
    wvel_FCME[m] = wvel_FC[m];
  }
  
  for(CellIterator iter = patch->getExtraCellIterator(); !iter.done();
      iter++){
    IntVector curcell = *iter;
    //__________________________________
    //  T  O  P -- B  E  T  A      
    //  Note this includes b[m][m]
    //  You need to make sure that mom_exch_coeff[m][m] = 0
    //   - form off diagonal terms of (a) 
    if (curcell.y() < (patch->getCellHighIndex()).y()-1) {
      IntVector adjcell(curcell.x(),curcell.y()+1,curcell.z()); 
      
      for(int m = 0; m < numMatls; m++) {
	for(int n = 0; n < numMatls; n++) {
	  tmp = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][curcell]) * 
	    K[n][m];
	  
	  beta[m][n] = delT * tmp/
	    (rho_micro_CC[m][curcell] + rho_micro_CC[m][adjcell]);
	  
	  a[m][n] = -beta[m][n];
	}
      }
      //__________________________________
      //  F  O  R  M     M  A  T  R  I  X   (a)
      //  - Diagonal terms      
      for(int m = 0; m < numMatls; m++) {
	a[m][m] = 1.;
	for(int n = 0; n < numMatls; n++) {
	  a[m][m] +=  beta[m][n];
	}
      }
      //__________________________________
      //    F  O  R  M     R  H  S  (b)     
      for(int m = 0; m < numMatls; m++) {
	b[m] = 0.0;
	for(int n = 0; n < numMatls; n++)  {
	  b[m] += beta[m][n] * (vvel_FC[n][*iter] - vvel_FC[m][*iter]);
	}
      }
      //__________________________________
      //      S  O  L  V  E  
      //   - backout velocities              
      itworked = a.solve(b);
      for(int m = 0; m < numMatls; m++)  {
	vvel_FCME[m][*iter] = vvel_FC[m][*iter] + b[m];
      }
    }
    //__________________________________
    //  R I G H T -- B  E  T  A      
    //  Note this includes b[m][m]
    //  You need to make sure that mom_exch_coeff[m][m] = 0
    //   - form off diagonal terms of (a)
    if (curcell.x() < (patch->getCellHighIndex()).x()-1)  {
      IntVector adjcell(curcell.x()+1,curcell.y(),curcell.z()); 
      for(int m = 0; m < numMatls; m++)  {
	for(int n = 0; n < numMatls; n++)  {
	  tmp = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][curcell]) * 
	    K[n][m];
	  
	  beta[m][n] = delT * tmp/
	    (rho_micro_CC[m][curcell] + rho_micro_CC[m][adjcell]);
	  
	  a[m][n] = -beta[m][n];
	}
      }
      /*__________________________________
       *  F  O  R  M     M  A  T  R  I  X   (a)
       * - Diagonal terms
       *___________________________________*/
      for(int m = 0; m < numMatls; m++) {
	a[m][m] = 1.;
	for(int n = 0; n < numMatls; n++) {
	  a[m][m] +=  beta[m][n];
	}
      }
      //__________________________________
      //    F  O  R  M     R  H  S  (b) 
      for(int m = 0; m < numMatls; m++)  {
	b[m] = 0.0;
	for(int n = 0; n < numMatls; n++)  {
	  b[m] += beta[m][n] * (uvel_FC[n][*iter] - uvel_FC[m][*iter]);
	}
      }
      //__________________________________
      //      S  O  L  V  E
      //   - backout velocities
      itworked = a.solve(b);
      for(int m = 0; m < numMatls; m++) {
	uvel_FCME[m][*iter] = uvel_FC[m][*iter] + b[m];
      }
    }
    //__________________________________
    //  F R O N T -- B  E  T  A      
    //  Note this includes b[m][m]
    //  You need to make sure that mom_exch_coeff[m][m] = 0
    //   - form off diagonal terms of (a)
    if (curcell.z() < (patch->getCellHighIndex()).z()-1)  {
      IntVector adjcell(curcell.x(),curcell.y(),curcell.z()+1); 
      for(int m = 0; m < numMatls; m++)  {
	for(int n = 0; n < numMatls; n++) {
	  tmp = (vol_frac_CC[n][adjcell] + vol_frac_CC[n][curcell]) *
	    K[n][m];
	  
	  beta[m][n] = delT * tmp/
	    (rho_micro_CC[m][curcell] + rho_micro_CC[m][adjcell]);
	  
	  a[m][n] = -beta[m][n];
	}
      }
      //__________________________________
      //  F  O  R  M     M  A  T  R  I  X   (a)
      // - Diagonal terms
      for(int m = 0; m < numMatls; m++) {
	a[m][m] = 1.;
	for(int n = 0; n < numMatls; n++) {
	  a[m][m] +=  beta[m][n];
	}
      }
      //__________________________________
      //    F  O  R  M     R  H  S  (b)
      for(int m = 0; m < numMatls; m++) {
	b[m] = 0.0;
	for(int n = 0; n < numMatls; n++) {
	  b[m] += beta[m][n] * (wvel_FC[n][*iter] - wvel_FC[m][*iter]);
	}
      }
      //__________________________________
      //      S  O  L  V  E
      //   - backout velocities
      itworked = a.solve(b);
      for(int m = 0; m < numMatls; m++) {
	wvel_FCME[m][*iter] = wvel_FC[m][*iter] + b[m];
      }
    }
  }
  
  for (int m = 0; m < numMatls; m++)  {
    setBC(uvel_FCME[m],"Velocity","x",patch);
    setBC(vvel_FCME[m],"Velocity","y",patch);
    setBC(wvel_FCME[m],"Velocity","z",patch);
  }
   /*`==========DEBUG============*/ 
  if (switchDebug_Exchange_FC ) {
    for (int m = 0; m < numMatls; m++)  {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();
    char description[50];
    sprintf(description, "Exchange_FC_before_BC_Mat_%d ",dwindex);
    printData_FC( patch,1, description, "uvel_FCME", uvel_FCME[m]);
    printData_FC( patch,1, description, "vvel_FCME", vvel_FCME[m]);
    printData_FC( patch,1, description, "wvel_FCME", wvel_FCME[m]);
    }
  }
  /*==========DEBUG============`*/
  for(int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();
    new_dw->put(uvel_FCME[m], lb->uvel_FCMELabel, dwindex, patch);
    new_dw->put(vvel_FCME[m], lb->vvel_FCMELabel, dwindex, patch);
    new_dw->put(wvel_FCME[m], lb->wvel_FCMELabel, dwindex, patch);
  }
  
}

/*---------------------------------------------------------------------
 Function~  ICE::computeDelPressAndUpdatePressCC--
 Purpose~
   This function calculates the change in pressure explicitly. 
 Note:  Units of delpress are [Pa]
 Reference:  Multimaterial Formalism eq. 1.5
 ---------------------------------------------------------------------  */
void ICE::computeDelPressAndUpdatePressCC(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing explicit delPress  \t\t\t ICE" << endl;
  int numMatls  = d_sharedState->getNumMatls();
  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());
  Vector dx     = patch->dCell();

  double vol    = dx.x()*dx.y()*dx.z();
  double invvol = 1./vol;

  CCVariable<double> q_CC,      q_advected;
  CCVariable<fflux> IFS,        OFS,    q_out,      q_in;
  CCVariable<eflux> IFE,        OFE,    q_out_EF,   q_in_EF;
  CCVariable<cflux> IFC,        OFC,    q_out_CF,   q_in_CF;                    
  CCVariable<double> pressure;
  CCVariable<double> delPress;
  CCVariable<double> press_CC;
  new_dw->get(pressure,        lb->press_equil_CCLabel,0, patch,Ghost::None,0);
  new_dw->allocate(delPress,   lb->delPress_CCLabel,0, patch);
  new_dw->allocate(press_CC,   lb->press_CCLabel,   0, patch);

  new_dw->allocate(q_CC,       lb->q_CCLabel,       0, patch);
  new_dw->allocate(q_advected, lb->q_advectedLabel, 0, patch);
  new_dw->allocate(IFS,        IFS_CCLabel,         0, patch);  
  new_dw->allocate(OFS,        OFS_CCLabel,         0, patch);  
  new_dw->allocate(IFE,        IFE_CCLabel,         0, patch);  
  new_dw->allocate(OFE,        OFE_CCLabel,         0, patch);  
  new_dw->allocate(IFC,        IFC_CCLabel,         0, patch);  
  new_dw->allocate(OFC,        OFC_CCLabel,         0, patch);
  new_dw->allocate(q_out,      q_outLabel,          0, patch);
  new_dw->allocate(q_out_EF,   q_out_EFLabel,       0, patch);
  new_dw->allocate(q_out_CF,   q_out_CFLabel,       0, patch);   
  new_dw->allocate(q_in,       q_inLabel,           0, patch);
  new_dw->allocate(q_in_EF,    q_in_EFLabel,        0, patch);
  new_dw->allocate(q_in_CF,    q_in_CFLabel,        0, patch);
  
  CCVariable<double> term1, term2, term3;
  new_dw->allocate(term1, lb->term3Label, 0, patch);
  new_dw->allocate(term2, lb->term3Label, 0, patch);
  new_dw->allocate(term3, lb->term3Label, 0, patch);

  term1.initialize(0.);
  term2.initialize(0.);
  term3.initialize(0.);
  delPress.initialize(0.0);
  
  for(int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();
    SFCXVariable<double> uvel_FC;
    SFCYVariable<double> vvel_FC;
    SFCZVariable<double> wvel_FC;
    CCVariable<double>  vol_frac;
    CCVariable<double>  rho_micro_CC;
    CCVariable<double>  speedSound;
    new_dw->get(uvel_FC, lb->uvel_FCMELabel, dwindex,  patch,Ghost::None, 0);
    new_dw->get(vvel_FC, lb->vvel_FCMELabel, dwindex,  patch,Ghost::None, 0);
    new_dw->get(wvel_FC, lb->wvel_FCMELabel, dwindex,  patch,Ghost::None, 0);
    new_dw->get(vol_frac,lb->vol_frac_CCLabel,dwindex, patch,Ghost::None, 0);                                                    
    new_dw->get(rho_micro_CC, lb->rho_micro_CCLabel,dwindex,patch,
		                                               Ghost::None, 0);
    new_dw->get(speedSound,lb->speedSound_CCLabel,dwindex,patch,
		                                               Ghost::None, 0);
    //__________________________________
    // Advection preprocessing
    // - divide vol_frac_cc/vol
    influxOutfluxVolume(uvel_FC,vvel_FC,wvel_FC,delT,patch,OFS,OFE,OFC,IFS,IFE,
			IFC);

    for(CellIterator iter = patch->getExtraCellIterator(); !iter.done();
	iter++) {
      q_CC[*iter] = vol_frac[*iter] * invvol;
    }
    //__________________________________
    //   First order advection of q_CC
    advectQFirst(q_CC, patch,OFS,OFE,OFC,IFS,IFE,IFC,q_out,q_out_EF,q_out_CF,
		 q_in,q_in_EF,q_in_CF,q_advected);
/*`==========DEBUG============*/ 
    if (switchDebug_explicit_press ) {
#if 1
    char description[50];
    sprintf(description, "middle_of_explicit_Pressure_Mat_%d ",dwindex);
    printData_FC( patch,1, description, "uvel_FC", uvel_FC);
    printData_FC( patch,1, description, "vvel_FC", vvel_FC);
    printData_FC( patch,1, description, "wvel_FC", wvel_FC);
#endif
    }
 /*==========DEBUG============`*/
    for(CellIterator iter = patch->getCellIterator(); !iter.done();  iter++) {
      //__________________________________
      //   Contributions from reactions
      //   to be filled in Be very careful with units
      term1[*iter] = 0.;
      
      //__________________________________
      //   Divergence of velocity * face area
      //   Be very careful with the units
      //   do the volume integral to check them
      //   See journal pg 171
      //   You need to divide by the cell volume
      //
      //  Note that sum(div (theta_k U^f_k) 
      //          =
      //  Advection(theta_k, U^f_k)
      //  This subtle point is discussed on pg
      //  190 of my Journal
      term2[*iter] -= q_advected[*iter];
      
      term3[*iter] += vol_frac[*iter] /(rho_micro_CC[*iter] *
					speedSound[*iter]*speedSound[*iter]);
    }
  }
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
    delPress[*iter] = (delT * term1[*iter] - term2[*iter])/(term3[*iter]);
    press_CC[*iter]  = pressure[*iter] + delPress[*iter];    
  }
  setBC(press_CC,"Pressure",patch);

  new_dw->put(delPress, lb->delPress_CCLabel, 0, patch);
  new_dw->put(press_CC, lb->press_CCLabel,    0, patch);
  
/*`==========DEBUG============*/ 
  if (switchDebug_explicit_press) {
    printData( patch, 1, "Bottom_of_explicit_Pressure ", "delPress_CC",  delPress);
    printData( patch, 1, "Bottom_of_explicit_Pressure",  "Press_CC",     press_CC);
  }
 /*==========DEBUG============`*/
}

/* ---------------------------------------------------------------------  
 Function~  ICE::computePressFC--
 Purpose~
    This function calculates the face centered pressure on each of the 
    cell faces for every cell in the computational domain and a single 
    layer of ghost cells.  This routine assume that there is a 
    single layer of ghostcells
    12/28/00    Changed the way press_FC is computed
  ---------------------------------------------------------------------  */
void ICE::computePressFC(
                    const ProcessorGroup*,   const Patch* patch,
			DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing press_face_MM \t\t\t\t ICE" << endl;
  int numMatls = d_sharedState->getNumMatls();
  double sum_rho, sum_rho_adj;
  double A;                                 
  
  vector<CCVariable<double> > rho_CC(numMatls);
  CCVariable<double> press_CC;
  new_dw->get(press_CC,lb->press_CCLabel, 0, patch, Ghost::None, 0);
  
  SFCXVariable<double> pressX_FC;
  SFCYVariable<double> pressY_FC;
  SFCZVariable<double> pressZ_FC;
  new_dw->allocate(pressX_FC,lb->pressX_FCLabel, 0, patch);
  new_dw->allocate(pressY_FC,lb->pressY_FCLabel, 0, patch);
  new_dw->allocate(pressZ_FC,lb->pressZ_FCLabel, 0, patch);

  // Compute the face centered velocities
  for(int m = 0; m < numMatls; m++)  {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();
    ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
    MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
    if(ice_matl){
      old_dw->get(rho_CC[m], lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
    }
    if(mpm_matl){
      new_dw->get(rho_CC[m], lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
    }
  }


  for(CellIterator iter = patch->getExtraCellIterator();!iter.done();iter++){
    IntVector curcell = *iter;
    //__________________________________
    //  T O P   F A C E
    if (curcell.y() < (patch->getCellHighIndex()).y()-1) {
      IntVector adjcell(curcell.x(),curcell.y()+1,curcell.z());
      sum_rho         =0.0;
      sum_rho_adj     = 0.0;
      for(int m = 0; m < numMatls; m++) {
	sum_rho      += (rho_CC[m][curcell] + d_SMALL_NUM);
	sum_rho_adj  += (rho_CC[m][adjcell] + d_SMALL_NUM);
      }
      
      A =  (press_CC[curcell]/sum_rho) + (press_CC[adjcell]/sum_rho_adj);
      pressY_FC[curcell+IntVector(0,1,0)] = A/((1/sum_rho)+(1.0/sum_rho_adj));
    }
    //__________________________________
    //  R I G H T   F A C E
    if (curcell.x() < (patch->getCellHighIndex()).x()-1)  {
      IntVector adjcell(curcell.x()+1,curcell.y(),curcell.z());
      
      sum_rho=0.0;
      sum_rho_adj  = 0.0;
      
      for(int m = 0; m < numMatls; m++) {
	sum_rho      += (rho_CC[m][curcell] + d_SMALL_NUM);
	sum_rho_adj  += (rho_CC[m][adjcell] + d_SMALL_NUM);
      }
      
      A =  (press_CC[curcell]/sum_rho) + (press_CC[adjcell]/sum_rho_adj);
      pressX_FC[curcell+IntVector(1,0,0)] = A/((1/sum_rho)+(1.0/sum_rho_adj));
    }
    //__________________________________
    //     F R O N T   F A C E 
    if (curcell.z() < (patch->getCellHighIndex()).z()-1) {
      IntVector adjcell(curcell.x(),curcell.y(),curcell.z()+1);
      
      sum_rho=0.0;
      sum_rho_adj  = 0.0;
      for(int m = 0; m < numMatls; m++) {
	sum_rho      += (rho_CC[m][curcell] + d_SMALL_NUM);
	sum_rho_adj  += (rho_CC[m][adjcell] + d_SMALL_NUM);
      }
     
      A =  (press_CC[curcell]/sum_rho) + (press_CC[adjcell]/sum_rho_adj);
      pressZ_FC[curcell+IntVector(0,0,1)]=A/((1/sum_rho)+(1.0/sum_rho_adj));
    }
  }

  setBC(pressX_FC,"Pressure",patch);
  setBC(pressY_FC,"Pressure",patch);
  setBC(pressZ_FC,"Pressure",patch);

  new_dw->put(pressX_FC,lb->pressX_FCLabel, 0, patch);
  new_dw->put(pressY_FC,lb->pressY_FCLabel, 0, patch);
  new_dw->put(pressZ_FC,lb->pressZ_FCLabel, 0, patch);
  
/*`==========TESTING==========*/ 
  if (switchDebug_PressFC) {
    printData_FC( patch,1,"press_FC",   "press_FC_RIGHT", pressX_FC);
    printData_FC( patch,1,"press_FC",   "press_FC_TOP",   pressY_FC);
    printData_FC( patch,1,"press_FC",   "press_FC_FRONT", pressZ_FC);
  }
  /*==========TESTING==========`*/
}

/* ---------------------------------------------------------------------
 Function~  ICE::accumulateMomentumSourceSinks--
 Purpose~   This function accumulates all of the sources/sinks of momentum
            which is added to the current value for the momentum to form
            the Lagrangian momentum
 ---------------------------------------------------------------------  */
void ICE::accumulateMomentumSourceSinks(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing accumulate_momentum_source_sinks_MM \t ICE" << endl;
  
  int numMatls  = d_sharedState->getNumMatls();
  
  IntVector right, left, top, bottom, front, back;
  delt_vartype delT; 
  Vector    dx, gravity;
  double    delX, delY, delZ;
  double    pressure_source, viscous_source, mass, vol;
  
  old_dw->get(delT, d_sharedState->get_delt_label());
  dx        = patch->dCell();
  gravity   = d_sharedState->getGravity();
  delX      = dx.x();
  delY      = dx.y();
  delZ      = dx.z();
  vol       = delX * delY * delZ;
  
  CCVariable<double>   rho_CC;
  CCVariable<Vector>   vel_CC;
  CCVariable<double>   visc_CC;
  CCVariable<double>   vol_frac;
  SFCXVariable<double> pressX_FC;
  SFCYVariable<double> pressY_FC;
  SFCZVariable<double> pressZ_FC;

  new_dw->get(pressX_FC,lb->pressX_FCLabel, 0, patch,Ghost::None, 0);
  new_dw->get(pressY_FC,lb->pressY_FCLabel, 0, patch,Ghost::None, 0);
  new_dw->get(pressZ_FC,lb->pressZ_FCLabel, 0, patch,Ghost::None, 0);

  for(int m = 0; m < numMatls; m++) {
     Material* matl = d_sharedState->getMaterial( m );
    int dwindex = matl->getDWIndex();
    ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
    MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
    if(ice_matl){
      old_dw->get(rho_CC, lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
    }
    if(mpm_matl){
      new_dw->get(rho_CC, lb->rho_CCLabel, dwindex, patch, Ghost::None, 0);
    }
/*`==========TESTING==========*/ 
//    old_dw->get(vel_CC,  lb->vel_CCLabel,      dwindex,patch,Ghost::None, 0);
//    old_dw->get(visc_CC, lb->viscosity_CCLabel,dwindex,patch,Ghost::None, 0);
 /*==========TESTING==========`*/
    new_dw->get(vol_frac,lb->vol_frac_CCLabel, dwindex,patch,Ghost::None, 0);

    CCVariable<Vector>   mom_source;
    SFCXVariable<double> tau_X_FC;
    SFCYVariable<double> tau_Y_FC;
    SFCZVariable<double> tau_Z_FC;
    new_dw->allocate(mom_source,  lb->mom_source_CCLabel,  dwindex, patch);
    new_dw->allocate(tau_X_FC,    lb->tau_X_FCLabel,       dwindex, patch);
    new_dw->allocate(tau_Y_FC,    lb->tau_Y_FCLabel,       dwindex, patch);
    new_dw->allocate(tau_Z_FC,    lb->tau_Z_FCLabel,       dwindex, patch);
 
    mom_source.initialize(Vector(0.,0.,0.));

    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
      mass = rho_CC[*iter] * vol;
      right    = *iter + IntVector(1,0,0);
      left     = *iter + IntVector(0,0,0);
      top      = *iter + IntVector(0,1,0);
      bottom   = *iter + IntVector(0,0,0);
      front    = *iter + IntVector(0,0,1);
      back     = *iter + IntVector(0,0,0);
      //__________________________________
      //    X - M O M E N T U M 
      pressure_source = (pressX_FC[right] - pressX_FC[left]) * vol_frac[*iter];
       
#if 0
      // tau variables are really vector quantities and need to be
      // stored as SFCXVariable<Vector>.  But for now they are not
      // being used.
      viscous_source  = tau_X_FC[*iter+IntVector(1,0,0)] - 
	tau_X_FC[*iter+IntVector(0,0,0)] + 
	tau_X_FC[*iter+IntVector(0,1,0)]  - 
	tau_X_FC[*iter+IntVector(0,0,0)] + 
	tau_X_FC[*iter+IntVector(0,0,1)] - 
	tau_X_FC[*iter+IntVector(0,0,0)];
#endif
      mom_source[*iter].x( (-pressure_source * delY * delZ +
			    mass * gravity.x()) * delT );
      //__________________________________
      //    Y - M O M E N T U M
       pressure_source = (pressY_FC[top] - pressY_FC[bottom])* vol_frac[*iter];
#if 0
      // tau variables are really vector quantities and need to be
      // stored as SFCXVariable<Vector>.  But for now they are not
      // being used.
      viscous_source  = tau_X_FC[*iter+IntVector(1,0,0)] - 
	tau_X_FC[*iter+IntVector(0,0,0)] + 
	tau_X_FC[*iter+IntVector(0,1,0)]  - 
	tau_X_FC[*iter+IntVector(0,0,0)] + 
	tau_X_FC[*iter+IntVector(0,0,1)] - 
	tau_X_FC[*iter+IntVector(0,0,0)];
#endif
      mom_source[*iter].y( (-pressure_source * delX * delZ +
			       mass * gravity.y()) * delT );
      //__________________________________
      //    Z - M O M E N T U M
      pressure_source = (pressZ_FC[front] - pressZ_FC[back]) * vol_frac[*iter];
#if 0
      // tau variables are really vector quantities and need to be
      // stored as SFCXVariable<Vector>.  But for now they are not
      // being used.
      viscous_source  = tau_X_FC[*iter+IntVector(1,0,0)] - 
	tau_X_FC[*iter+IntVector(0,0,0)] + 
	tau_X_FC[*iter+IntVector(0,1,0)]  - 
	tau_X_FC[*iter+IntVector(0,0,0)] + 
	tau_X_FC[*iter+IntVector(0,0,1)] - 
	tau_X_FC[*iter+IntVector(0,0,0)];
#endif
      mom_source[*iter].z( (-pressure_source * delX * delY +
			       mass * gravity.z()) * delT );
    }

    new_dw->put(mom_source, lb->mom_source_CCLabel, dwindex, patch);
    /*`==========TESTING==========*/ 
    if (switchDebugSource_Sink) {
      char description[50];
      sprintf(description, "sources/sinks_Mat_%d",dwindex);
      printVector( patch, 1, description,    "xmom_source", 0, mom_source);
      printVector( patch, 1, description,    "ymom_source", 1, mom_source);
      printVector( patch, 1, description,    "zmom_source", 2, mom_source);
    }
    /*==========TESTING==========`*/
  }
}

/* --------------------------------------------------------------------- 
 Function~  ICE::accumulateEnergySourceSinks--
 Purpose~   This function accumulates all of the sources/sinks of energy
            which is added to the current value for the energy to form
            the Lagrangian energy  

 Currently the kinetic energy isn't included.
 This is the routine where you would add additional sources/sinks of energy                 
 ---------------------------------------------------------------------  */
void ICE::accumulateEnergySourceSinks(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing accumulate_energy_source_sinks \t\t ICE" << endl;

  int numMatls = d_sharedState->getNumMatls();

  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());
  Vector dx = patch->dCell();
  double A, B, vol=dx.x()*dx.y()*dx.z();

  CCVariable<double> rho_micro_CC;
  CCVariable<double> speedSound;
  CCVariable<double> vol_frac;
  CCVariable<double> press_CC;
  CCVariable<double> delPress;

  new_dw->get(press_CC,lb->press_CCLabel,    0, patch,Ghost::None, 0);
  new_dw->get(delPress,lb->delPress_CCLabel, 0, patch,Ghost::None, 0);

  for(int m = 0; m < numMatls; m++) {
    Material* matl = d_sharedState->getMaterial( m );
    int dwindex    = matl->getDWIndex();                 
    new_dw->get(rho_micro_CC,lb->rho_micro_CCLabel,dwindex,patch,
		                                              Ghost::None, 0);                                            
    new_dw->get(speedSound,lb->speedSound_CCLabel,dwindex,patch,
		                                              Ghost::None, 0); 
    new_dw->get(vol_frac,lb->vol_frac_CCLabel,dwindex,patch,Ghost::None, 0);
    CCVariable<double> int_eng_source;
    new_dw->allocate(int_eng_source,lb->int_eng_source_CCLabel,dwindex,patch);
    
    //__________________________________
    //   Compute int_eng_source in 
    //   interior cells
    int_eng_source.initialize(0.);
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      A = vol * vol_frac[*iter] * press_CC[*iter];
      B = rho_micro_CC[*iter]   * speedSound[*iter] * speedSound[*iter];
      int_eng_source[*iter] = (A/B) * delPress[*iter];
    }
   /*`==========TESTING==========*/
    if (switchDebugSource_Sink) {
      char description[50];
      sprintf(description, "sources/sinks_Mat_%d ",dwindex);
      printData( patch, 1, description,    "int_eng_source",  int_eng_source);
    }
    /*==========TESTING==========`*/
    new_dw->put(int_eng_source,lb->int_eng_source_CCLabel,dwindex,patch);
  }
}

/* ---------------------------------------------------------------------
 Function~  ICE::computeLagrangianValues--
 Purpose~
   This function calculates the The cell-centered, time n+1, 
   lagrangian mass momentum and energy
 Note:      Only loop over ICE materials, mom_L for MPM is computed
            prior to this function
 ---------------------------------------------------------------------  */
void ICE::computeLagrangianValues(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing Lagrangian mass, momentum and energy \t ICE" << endl;

  int numICEMatls = d_sharedState->getNumICEMatls();
  int numALLMatls = d_sharedState->getNumMatls();
  Vector    dx = patch->dCell();
  
  // Compute the Lagrangian quantities
  for(int m = 0; m < numICEMatls; m++) {
    ICEMaterial* matl = d_sharedState->getICEMaterial( m );
    int dwindex = matl->getDWIndex();
    CCVariable<double> rho_CC,cv_CC,temp_CC;
    CCVariable<Vector> vel_CC;
    CCVariable<double> int_eng_source;
    CCVariable<Vector> mom_source;
    CCVariable<Vector> mom_L;
    CCVariable<double> int_eng_L, mass_L;

    old_dw->get(rho_CC,  lb->rho_CCLabel,     dwindex,patch,Ghost::None, 0);
    old_dw->get(vel_CC,  lb->vel_CCLabel,     dwindex,patch,Ghost::None, 0);
    old_dw->get(cv_CC,   lb->cv_CCLabel,      dwindex,patch,Ghost::None, 0);
    old_dw->get(temp_CC, lb->temp_CCLabel,    dwindex,patch,Ghost::None, 0);
    new_dw->get(mom_source,     lb->mom_source_CCLabel,dwindex,patch,
		                                              Ghost::None, 0);
    new_dw->get(int_eng_source, lb->int_eng_source_CCLabel,dwindex,patch,
		                                              Ghost::None, 0);
    new_dw->allocate(mom_L,     lb->mom_L_CCLabel,     dwindex,patch);
    new_dw->allocate(int_eng_L, lb->int_eng_L_CCLabel, dwindex,patch);
    new_dw->allocate(mass_L,    lb->mass_L_CCLabel,    dwindex,patch);
    
    double vol = dx.x()*dx.y()*dx.z();
    for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); 
	iter++) {
      double mass = rho_CC[*iter] * vol;
      mass_L[*iter] = mass; 
      // +  mass_source[*iter];
      
      mom_L[*iter] = vel_CC[*iter] * mass
	//- vel_CC[*iter] * mass_source[*iter]
	+ mom_source[*iter];
      
      int_eng_L[*iter] = mass * cv_CC[*iter] * temp_CC[*iter]
	//-cv_CC[*iter] * temp_CC * mass_source[*iter]
	+ int_eng_source[*iter];
    }
    new_dw->put(mom_L,     lb->mom_L_CCLabel,     dwindex,patch);
    new_dw->put(int_eng_L, lb->int_eng_L_CCLabel, dwindex,patch);
    new_dw->put(mass_L,    lb->mass_L_CCLabel,    dwindex,patch);
   }
  /*`==========DEBUG============*/ 
  // Dump out all the matls data
  if (switchDebugLagrangianValues ) {
    int gc;
    for(int m = 0; m < numALLMatls; m++) {
      Material* matl = d_sharedState->getMaterial( m );
      ICEMaterial* ice_matl = dynamic_cast<ICEMaterial*>(matl);
      MPMMaterial* mpm_matl = dynamic_cast<MPMMaterial*>(matl);
      CCVariable<Vector > mom_L;
      CCVariable<double > int_eng_L;
      int dwindex = matl->getDWIndex();
      if(ice_matl)  {
        gc = 1;
        new_dw->get(int_eng_L, lb->int_eng_L_CCLabel,  dwindex,patch,Ghost::None,0);
        new_dw->get(mom_L,     lb->mom_L_CCLabel,      dwindex,patch,Ghost::None,0);
      }
      if(mpm_matl)  {
        gc = 0;
        new_dw->get(int_eng_L, MIlb->int_eng_L_CCLabel,dwindex,patch,Ghost::None,0);
        new_dw->get(mom_L,     MIlb->mom_L_CCLabel,    dwindex,patch,Ghost::None,0);
      }
      char description[50];
      sprintf(description, "Bot_Lagrangian_Values_Mat_%d ",dwindex);
      printVector( patch,gc, description, "xmom_L_CC", 0, mom_L);
      printVector( patch,gc, description, "ymom_L_CC", 1, mom_L);
      printVector( patch,gc, description, "zmom_L_CC", 2, mom_L);
      printData(   patch,gc, description, "int_eng_L_CC",int_eng_L);
    }
  }
    /*==========DEBUG============`*/
}

/*---------------------------------------------------------------------
 Function~  ICE::addExchangeToMomentumAndEnergy--
 Purpose~
   This function adds the energy exchange contribution to the 
   existing cell-centered lagrangian temperature

 Prerequisites:
            The face centered velocity for each material without
            the exchange must be solved prior to this routine.
            
                   (A)                              (X)
| (1+b12 + b13)     -b12          -b23          |   |del_data_CC[1]  |    
|                                               |   |                |    
| -b21              (1+b21 + b23) -b32          |   |del_data_CC[2]  |    
|                                               |   |                | 
| -b31              -b32          (1+b31 + b32) |   |del_data_CC[2]  |

                        =
                        
                        (B)
| b12( data_CC[2] - data_CC[1] ) + b13 ( data_CC[3] -data_CC[1])    | 
|                                                                   |
| b21( data_CC[1] - data_CC[2] ) + b23 ( data_CC[3] -data_CC[2])    | 
|                                                                   |
| b31( data_CC[1] - data_CC[3] ) + b32 ( data_CC[2] -data_CC[3])    |           
 
 - set *_L_ME arrays = *_L arrays
 - convert flux variables
 Steps for each cell;
    1) Comute the beta coefficients
    2) Form and A matrix and B vector
    3) Solve for del_data_CC[*]
    4) Add del_data_CC[*] to the appropriate Lagrangian data
 - apply Boundary conditions to vel_CC and Temp_CC
 - Stuff fluxes mom_L_ME and int_eng_L_ME back into dw
 
 References: see "A Cell-Centered ICE method for multiphase flow simulations"
 by Kashiwa, above equation 4.13.
 ---------------------------------------------------------------------  */
void ICE::addExchangeToMomentumAndEnergy(
            const ProcessorGroup*,  const Patch* patch,
	     DataWarehouseP& old_dw, DataWarehouseP& new_dw)
{
  cout << "Doing Heat and momentum exchange \t\t ICE" << endl;

  int     numMatls  = d_sharedState->getNumICEMatls();
  double  tmp;
  int     itworked;
  Vector dx         = patch->dCell();
  double vol        = dx.x()*dx.y()*dx.z();
  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());

  vector<CCVariable<double> > rho_CC(numMatls);
  vector<CCVariable<double> > Temp_CC(numMatls);
  vector<CCVariable<double> > int_eng_L(numMatls);
  vector<CCVariable<double> > vol_frac_CC(numMatls);
  vector<CCVariable<double> > rho_micro_CC(numMatls);
  vector<CCVariable<double> > int_eng_L_ME(numMatls);
  vector<CCVariable<double> > cv_CC(numMatls);
  vector<CCVariable<Vector> > mom_L(numMatls);
  vector<CCVariable<Vector> > vel_CC(numMatls);
  vector<CCVariable<Vector> > mom_L_ME(numMatls);
  
  vector<double> b(numMatls);
  vector<double> mass(numMatls);
  DenseMatrix beta(numMatls,numMatls),acopy(numMatls,numMatls);
  DenseMatrix K(numMatls,numMatls),H(numMatls,numMatls),a(numMatls,numMatls);
  beta.zero();
  acopy.zero();
  K.zero();
  H.zero();
  a.zero();
    

  for(int m = 0; m < numMatls; m++)  {
    ICEMaterial* matl = d_sharedState->getICEMaterial( m );
    int dwindex = matl->getDWIndex();
    old_dw->get(rho_CC[m],   lb->rho_CCLabel,       dwindex,patch, Ghost::None,0);
    new_dw->get(mom_L[m],    lb->mom_L_CCLabel,     dwindex,patch, Ghost::None,0);
    new_dw->get(int_eng_L[m],lb->int_eng_L_CCLabel, dwindex,patch,
		                                                     Ghost::None,0);
    new_dw->get(vol_frac_CC[m], lb->vol_frac_CCLabel,dwindex,patch, Ghost::None,0);
    new_dw->get(rho_micro_CC[m],lb->rho_micro_CCLabel,dwindex,patch,
		                                                     Ghost::None,0);           
    old_dw->get(cv_CC[m],lb->cv_CCLabel,dwindex,patch, Ghost::None, 0);

    new_dw->allocate( vel_CC[m],     lb->vel_CCLabel,          dwindex, patch);
    new_dw->allocate( mom_L_ME[m],   lb->mom_L_ME_CCLabel,     dwindex, patch);
    new_dw->allocate(int_eng_L_ME[m],lb->int_eng_L_ME_CCLabel, dwindex, patch);
    new_dw->allocate( Temp_CC[m],    lb->temp_CCLabel,         dwindex, patch);
  }
  for (int i = 0; i < numMatls; i++ )  {
      K[numMatls-1-i][i] = d_K_mom[i];
      H[numMatls-1-i][i] = d_K_heat[i];
  }      
  //__________________________________
  // Convert vars. flux -> primitive 
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
    for (int m = 0; m < numMatls; m++) {
      mass[m] = rho_CC[m][*iter] * vol;
      Temp_CC[m][*iter] = int_eng_L[m][*iter]/(mass[m]*cv_CC[m][*iter]);
      vel_CC[m][*iter]  =  mom_L[m][*iter]/mass[m];
    }  
  }

  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
    //   Form BETA matrix (a), off diagonal terms
    //  The beta and (a) matrix is common to all momentum exchanges
    for(int m = 0; m < numMatls; m++)  {
      tmp    = rho_micro_CC[m][*iter];
      for(int n = 0; n < numMatls; n++) {
	beta[m][n] = delT * vol_frac_CC[n][*iter] * K[n][m]/tmp;
	a[m][n] = -beta[m][n];
      }
    }
    
    //   Form matrix (a) diagonal terms
    for(int m = 0; m < numMatls; m++) {
      a[m][m] = 1.0;
      for(int n = 0; n < numMatls; n++) {
	a[m][m] +=  beta[m][n];
      }
    }
    //---------- X - M O M E N T U M
    // -  F O R M   R H S   (b)
    // -  push a copy of (a) into the solver
    // -  Adde exchange contribution to orig value
    for(int m = 0; m < numMatls; m++) {
      b[m] = 0.0;

      for(int n = 0; n < numMatls; n++) {
	b[m] += beta[m][n] *
	  (vel_CC[n][*iter].x() - vel_CC[m][*iter].x());
      }
    }
    acopy = a;
    itworked = acopy.solve(b);

    for(int m = 0; m < numMatls; m++) {
        vel_CC[m][*iter].x( vel_CC[m][*iter].x() + b[m] );
    }
    
    //---------- Y - M O M E N T U M
    // -  F O R M   R H S   (b)
    // -  push a copy of (a) into the solver
    // -  Adde exchange contribution to orig value
    for(int m = 0; m < numMatls; m++) {
      b[m] = 0.0;

      for(int n = 0; n < numMatls; n++) {
	b[m] += beta[m][n] *
	  (vel_CC[n][*iter].y() - vel_CC[m][*iter].y());
      }
    }
    acopy    = a;
    itworked = acopy.solve(b);
    
    for(int m = 0; m < numMatls; m++)   {
        vel_CC[m][*iter].y( vel_CC[m][*iter].y() + b[m] );
    }

    //---------- Z - M O M E N T U M
    // -  F O R M   R H S   (b)
    // -  push a copy of (a) into the solver
    // -  Adde exchange contribution to orig value
    for(int m = 0; m < numMatls; m++)  {
      b[m] = 0.0;
       
      for(int n = 0; n < numMatls; n++) {
	b[m] += beta[m][n] *
	  (vel_CC[n][*iter].z() - vel_CC[m][*iter].z());
      }
    }    
    acopy    = a;
    itworked = acopy.solve(b);
    
    for(int m = 0; m < numMatls; m++)  {
      vel_CC[m][*iter].z( vel_CC[m][*iter].z() + b[m] );
    }  
    //---------- E N E R G Y   E X C H A N G E
    //   Form BETA matrix (a) off diagonal terms
    for(int m = 0; m < numMatls; m++) {
      tmp = cv_CC[m][*iter]*rho_micro_CC[m][*iter];
      for(int n = 0; n < numMatls; n++)  {
	beta[m][n] = delT * vol_frac_CC[n][*iter] * H[n][m]/tmp;
	a[m][n] = -beta[m][n];
      }
    }
    //   Form matrix (a) diagonal terms
    for(int m = 0; m < numMatls; m++) {
      a[m][m] = 1.;
      for(int n = 0; n < numMatls; n++)   {
	a[m][m] +=  beta[m][n];
      }
    }
    // -  F O R M   R H S   (b)
    for(int m = 0; m < numMatls; m++)  {
      b[m] = 0.0;
     
     for(int n = 0; n < numMatls; n++) {
	b[m] += beta[m][n] *
	  (Temp_CC[n][*iter] - Temp_CC[m][*iter]);
      }
    }
    //     S O L V E, Add exchange contribution to orig value
    itworked = a.solve(b);
    
    for(int m = 0; m < numMatls; m++) {
      Temp_CC[m][*iter] = Temp_CC[m][*iter] + b[m];
    }
  }
  //__________________________________
  //  Set the Boundary condiitions
  for (int m = 0; m < numMatls; m++)  {
    setBC(vel_CC[m],"Velocity",patch);
    setBC(Temp_CC[m],"Temperature",patch);
  }
  //__________________________________
  // Convert vars. primitive-> flux 
  for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); iter++){
    for (int m = 0; m < numMatls; m++) {
      mass[m] = rho_CC[m][*iter] * vol;
      int_eng_L_ME[m][*iter] = Temp_CC[m][*iter] * cv_CC[m][*iter] * mass[m];
      mom_L_ME[m][*iter]     = vel_CC[m][*iter] * mass[m];
    }  
  }
   
  for(int m = 0; m < numMatls; m++) {
    ICEMaterial* matl = d_sharedState->getICEMaterial( m );
    int dwindex = matl->getDWIndex();
    new_dw->put(mom_L_ME[m],    lb->mom_L_ME_CCLabel,   dwindex, patch);
    new_dw->put(int_eng_L_ME[m],lb->int_eng_L_ME_CCLabel,dwindex, patch);
  }

}

/* --------------------------------------------------------------------- 
 Function~  ICE::advectAndAdvanceInTime--
 Purpose~
   This function calculates the The cell-centered, time n+1, mass, momentum
   and internal energy

Need to include kinetic energy 
 ---------------------------------------------------------------------  */
void ICE::advectAndAdvanceInTime(
            const ProcessorGroup*, const Patch* patch,
	     DataWarehouseP& old_dw,DataWarehouseP& new_dw)
{

  cout << "Doing Advect and Advance in Time \t\t ICE" << endl;
  delt_vartype delT;
  old_dw->get(delT, d_sharedState->get_delt_label());

  Vector dx = patch->dCell();
  double vol = dx.x()*dx.y()*dx.z(),mass;
  double invvol = 1.0/vol;

  CCVariable<double> int_eng_L_ME, mass_L;
  CCVariable<Vector> mom_L_ME;
  CCVariable<double> speedSound,cv_old;
  
  SFCXVariable<double> uvel_FC;
  SFCYVariable<double> vvel_FC;
  SFCZVariable<double> wvel_FC;
  
  // These arrays get re-used for each material, and for each
  // advected quantity
  CCVariable<double> q_CC, q_advected;
  CCVariable<fflux> IFS, OFS, q_out, q_in;
  CCVariable<eflux> IFE, OFE, q_out_EF, q_in_EF;
  CCVariable<cflux> IFC, OFC, q_out_CF, q_in_CF;
  
  new_dw->allocate(q_CC,       lb->q_CCLabel,       0, patch);
  new_dw->allocate(q_advected, lb->q_advectedLabel, 0, patch);
  new_dw->allocate(IFS,        IFS_CCLabel,         0, patch);
  new_dw->allocate(OFS,        OFS_CCLabel,         0, patch);
  new_dw->allocate(IFE,        IFE_CCLabel,         0, patch);
  new_dw->allocate(OFE,        OFE_CCLabel,         0, patch);
  new_dw->allocate(IFC,        IFE_CCLabel,         0, patch);
  new_dw->allocate(OFC,        OFE_CCLabel,         0, patch);
  new_dw->allocate(q_out,      q_outLabel,          0, patch);
  new_dw->allocate(q_out_EF,   q_out_EFLabel,       0, patch);
  new_dw->allocate(q_out_CF,   q_out_CFLabel,       0, patch);
  new_dw->allocate(q_in,       q_inLabel,           0, patch);
  new_dw->allocate(q_in_EF,    q_in_EFLabel,        0, patch);
  new_dw->allocate(q_in_CF,    q_in_CFLabel,        0, patch);
  
  for (int m = 0; m < d_sharedState->getNumICEMatls(); m++ ) {
    ICEMaterial* ice_matl = d_sharedState->getICEMaterial(m);
    int dwindex = ice_matl->getDWIndex();
    old_dw->get(cv_old,lb->cv_CCLabel,   dwindex, patch,Ghost::None, 0);     
    new_dw->get(uvel_FC,   lb->uvel_FCMELabel,   dwindex,patch,Ghost::None,0);
    new_dw->get(vvel_FC,   lb->vvel_FCMELabel,   dwindex,patch,Ghost::None,0);
    new_dw->get(wvel_FC,   lb->wvel_FCMELabel,   dwindex,patch,Ghost::None,0);
    new_dw->get(mom_L_ME,  lb->mom_L_ME_CCLabel, dwindex,patch,Ghost::None,0);
    new_dw->get(mass_L,    lb->mass_L_CCLabel,   dwindex,patch,Ghost::None,0);
    new_dw->get(int_eng_L_ME,lb->int_eng_L_ME_CCLabel,dwindex,patch,
		Ghost::None,0);
    CCVariable<double> rho_CC, visc_CC, cv,temp;
    CCVariable<Vector> vel_CC;
    new_dw->allocate(rho_CC, lb->rho_CCLabel,        dwindex,patch);
    new_dw->allocate(temp,   lb->temp_CCLabel,       dwindex,patch);
    new_dw->allocate(cv,     lb->cv_CCLabel,         dwindex,patch);
    new_dw->allocate(vel_CC, lb->vel_CCLabel,        dwindex,patch);
    new_dw->allocate(visc_CC,lb->viscosity_CCLabel,  dwindex,patch);
 
    cv = cv_old;
    
    //   Advection preprocessing
    influxOutfluxVolume(uvel_FC,vvel_FC,wvel_FC,delT,patch,OFS,OFE,OFC,IFS,
			IFE,IFC);
    
    // outflowVolCentroid goes here if doing second order
    //outflowVolCentroid(uvel_FC,vvel_FC,wvel_FC,delT,dx,
    //           r_out_x, r_out_y, r_out_z,
    //           r_out_x_CF, r_out_y_CF, r_out_z_CF);
    
    // Advect mass and backout rho_CC
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done();iter++){
      q_CC[*iter] = mass_L[*iter] * invvol;
    }
    
    advectQFirst(q_CC,patch,OFS,OFE,OFC,IFS,IFE,IFC,q_out,q_out_EF,q_out_CF,
		 q_in,q_in_EF,q_in_CF,q_advected);
    
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
      rho_CC[*iter] = (mass_L[*iter] + q_advected[*iter]) * invvol;
    }
    
    // Advect X momentum and backout vel_CC.x()
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done(); iter++){
      q_CC[*iter] = mom_L_ME[*iter].x() * invvol;
    }
    
    advectQFirst(q_CC,patch,OFS,OFE,OFC,IFS,IFE,IFC,q_out,q_out_EF,q_out_CF,
		 q_in,q_in_EF,q_in_CF,q_advected);

    for(CellIterator iter = patch->getCellIterator(); !iter.done();  iter++){
      mass = rho_CC[*iter] * vol;
      vel_CC[*iter].x( (mom_L_ME[*iter].x() + q_advected[*iter])/mass );
    }
    // Advect Y momentum and backout vvel_CC
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done(); iter++){
      q_CC[*iter] = mom_L_ME[*iter].y() * invvol;
    }
    
    advectQFirst(q_CC,patch,OFS,OFE,OFC,IFS,IFE,IFC,q_out,q_out_EF,q_out_CF,
		 q_in,q_in_EF, q_in_CF, q_advected); 
    
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      mass = rho_CC[*iter] * vol;
      vel_CC[*iter].y( (mom_L_ME[*iter].y() + q_advected[*iter])/mass );
    }
    
    // Advect Z momentum and backout wvel_CC
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done(); iter++){
      q_CC[*iter] = mom_L_ME[*iter].z() * invvol;
    }
    
    advectQFirst(q_CC, patch,OFS,OFE,OFC,IFS,IFE,IFC,q_out,q_out_EF,q_out_CF,
		 q_in,q_in_EF,q_in_CF,q_advected);
                                        
    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
      mass = rho_CC[*iter] * vol;
      vel_CC[*iter].z( (mom_L_ME[*iter].z() + q_advected[*iter])/mass );
    }

    // Advect internal energy and backout Temp_CC
    for(CellIterator iter=patch->getExtraCellIterator(); !iter.done(); iter++){
      q_CC[*iter] = int_eng_L_ME[*iter] * invvol;
    }
    
    advectQFirst(q_CC,patch, OFS,OFE, OFC,IFS,IFE,IFC,q_out,q_out_EF,q_out_CF,
		 q_in,q_in_EF,q_in_CF,q_advected);

    for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){
      mass = rho_CC[*iter] * vol;
      temp[*iter] = (int_eng_L_ME[*iter] + q_advected[*iter])/(mass*cv[*iter]);
    }

    setBC(rho_CC,   "Density",              patch);
    setBC(temp,     "Temperature",          patch);
    setBC(vel_CC,   "Velocity",             patch);

    /*`==========DEBUG============*/  
    if (switchDebug_advance_advect ) {
    char description[50];
    sprintf(description, "AFTER_Advection_after_BC_Mat_%d ",dwindex);
    printVector( patch,1, description, "xmom_L_CC", 0, mom_L_ME);
    printVector( patch,1, description, "ymom_L_CC", 1, mom_L_ME);
    printVector( patch,1, description, "zmom_L_CC", 2, mom_L_ME);
    printData( patch,1, description,   "int_eng_L_CC",int_eng_L_ME);
    printData( patch,1, description,   "rho_CC",      rho_CC);
    printData( patch,1, description,   "Temp_CC",temp);
    printVector( patch,1, description, "uvel_CC", 0, vel_CC);
    printVector( patch,1, description, "vvel_CC", 1, vel_CC);
    printVector( patch,1, description, "wvel_CC", 2, vel_CC);
    }
    /*==========DEBUG============`*/
    
    new_dw->put(rho_CC, lb->rho_CCLabel,  dwindex,patch);
    new_dw->put(vel_CC, lb->vel_CCLabel, dwindex,patch);
    new_dw->put(temp,   lb->temp_CCLabel, dwindex,patch);
    
    // These are carried forward variables, they don't change
    new_dw->put(visc_CC,lb->viscosity_CCLabel,dwindex,patch);
    new_dw->put(cv,     lb->cv_CCLabel,       dwindex,patch);
  }
  
}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--
 Purpose~   Takes care of Density_CC, Pressure_CC and Temperature_CC
 ---------------------------------------------------------------------  */
void ICE::setBC(CCVariable<double>& variable, const string& kind, 
		const Patch* patch)
{
  
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);

    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;

    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }

    if (bc_base == 0)
      continue;
    
    if (bc_base->getType() == "Pressure") {
      PressureBoundCond* bc = dynamic_cast<PressureBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
      
    }
    if (bc_base->getType() == "Density") {
      DensityBoundCond* bc = dynamic_cast<DensityBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
    }
    if (bc_base->getType() == "Temperature") {
      TemperatureBoundCond* bc = dynamic_cast<TemperatureBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
      
    }
  }
}


/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--        
 Purpose~   Takes care of Velocity_CC Boundary conditions
 ---------------------------------------------------------------------  */
void ICE::setBC(CCVariable<Vector>& variable, const string& kind, 
		const Patch* patch) 
{
  IntVector  low, hi;
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }

    if (bc_base == 0)
      continue;
    
    if (bc_base->getType() == "Velocity") {
      VelocityBoundCond* bc = dynamic_cast<VelocityBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
       //__________________________________
       // set BC value = -interior value
      if (bc->getKind() == "NegInterior") {
          low = variable.getLowIndex();
          hi  = variable.getHighIndex();
          switch (face) {
          case Patch::xplus:
            for (int j = low.y(); j<hi.y(); j++) {
              for (int k = low.z(); k<hi.z(); k++) {
                variable[IntVector(hi.x()-1,j,k)] = -variable[IntVector(hi.x()-2,j,k)];
              }
            }
            break;
          case Patch::xminus:
            for (int j = low.y(); j<hi.y(); j++) {
              for (int k = low.z(); k<hi.z(); k++) {
                variable[IntVector(low.x(),j,k)] = -variable[IntVector(low.x()+1,j,k)];
              }
            }
            break;
          case Patch::yplus:
            for (int i = low.x(); i<hi.x(); i++) {
              for (int k = low.z(); k<hi.z(); k++) {
                variable[IntVector(i,hi.y()-1,k)] = -variable[IntVector(i,hi.y()-2,k)];
              }
            }
            break;
          case Patch::yminus:
            for (int i = low.x(); i<hi.x(); i++) {
              for (int k = low.z(); k<hi.z(); k++) {
                variable[IntVector(i,low.y(),k)] = -variable[IntVector(i,low.y()+1,k)];
              }
            }
            break;
          case Patch::zplus:
            for (int i = low.x(); i<hi.x(); i++) {
              for (int j = low.y(); j<hi.y(); j++) {
                variable[IntVector(i,j,hi.z()-1)] = -variable[IntVector(i,j,hi.z()-2)];
              }
            }
            break;
          case Patch::zminus:
            for (int i = low.x(); i<hi.x(); i++) {
              for (int j = low.y(); j<hi.y(); j++) {
                variable[IntVector(i,j,low.z())] = -variable[IntVector(i,j,low.z()+1)];
              }
            }
            break;
          case Patch::numFaces:
            break;
          case Patch::invalidFace:
            break;        
        }  //end switch(face)
      }  //end NegInterior conditional
    }  // end velocity loop
  }  // end face loop
}

/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of Press_FC.x
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCXVariable<double>& variable, const string& kind, 
		const Patch* patch)
{

  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }

    if (bc_base == 0)
      continue;
    
    if (bc_base->getType() == "Pressure") {
      PressureBoundCond* bc = dynamic_cast<PressureBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
    }
  }
}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of vel_FC.x()
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCXVariable<double>& variable, const  string& kind, 
		const string& comp, const Patch* patch) 
{
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }
    
    if (bc_base == 0)
      continue;

    if (bc_base->getType() == "Velocity") {
      VelocityBoundCond* bc = dynamic_cast<VelocityBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") {
	if (comp == "x")
	  variable.fillFace(face,bc->getValue().x());
	if (comp == "y")
	  variable.fillFace(face,bc->getValue().y());
	if (comp == "z")
	  variable.fillFace(face,bc->getValue().z());
      }
      
      if (bc->getKind() == "Neumann") {
	if (comp == "x")
	  variable.fillFaceFlux(face,bc->getValue().x(),dx);
	if (comp == "y")
	  variable.fillFaceFlux(face,bc->getValue().y(),dx);
	if (comp == "z")
	  variable.fillFaceFlux(face,bc->getValue().z(),dx);
      }
    }
  }

}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of Press_FC.y
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCYVariable<double>& variable, const string& kind, 
		const Patch* patch)
{

  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }

    if (bc_base == 0)
      continue;
    
    if (bc_base->getType() == "Pressure") {
      PressureBoundCond* bc = dynamic_cast<PressureBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
    }
  }
}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of vel_FC.y()
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCYVariable<double>& variable, const  string& kind, 
		const string& comp, const Patch* patch) 
{
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }
    
    if (bc_base == 0)
      continue;

    if (bc_base->getType() == "Velocity") {
      VelocityBoundCond* bc = dynamic_cast<VelocityBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") {
	if (comp == "x")
	  variable.fillFace(face,bc->getValue().x());
	if (comp == "y")
	  variable.fillFace(face,bc->getValue().y());
	if (comp == "z")
	  variable.fillFace(face,bc->getValue().z());
      }
      
      if (bc->getKind() == "Neumann") {
	if (comp == "x")
	  variable.fillFaceFlux(face,bc->getValue().x(),dx);
	if (comp == "y")
	  variable.fillFaceFlux(face,bc->getValue().y(),dx);
	if (comp == "z")
	  variable.fillFaceFlux(face,bc->getValue().z(),dx);
      }
    }
  }

}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of Press_FC.z
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCZVariable<double>& variable, const string& kind, 
		const Patch* patch)
{

  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }
    if (bc_base == 0)
      continue;

    if (bc_base->getType() == "Pressure") {
      PressureBoundCond* bc = dynamic_cast<PressureBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") 
	variable.fillFace(face,bc->getValue());
      
      if (bc->getKind() == "Neumann") 
	variable.fillFaceFlux(face,bc->getValue(),dx);
    }
  }
}
/* --------------------------------------------------------------------- 
 Function~  ICE::setBC--      
 Purpose~   Takes care of vel_FC.z()
 ---------------------------------------------------------------------  */
void ICE::setBC(SFCZVariable<double>& variable, const  string& kind, 
		const string& comp, const Patch* patch) 
{
  Vector dx = patch->dCell();
  for(Patch::FaceType face = Patch::startFace;
      face <= Patch::endFace; face=Patch::nextFace(face)){
    vector<BoundCondBase* > bcs;
    bcs = patch->getBCValues(face);
    if (bcs.size() == 0) continue;
    
    BoundCondBase* bc_base = 0;
    for (int i = 0; i<(int)bcs.size(); i++ ) {
      if (bcs[i]->getType() == kind) {
	bc_base = bcs[i];
	break;
      }
    }

    if (bc_base == 0)
      continue;
    
    if (bc_base->getType() == "Velocity") {
      VelocityBoundCond* bc = dynamic_cast<VelocityBoundCond*>(bc_base);
      if (bc->getKind() == "Dirichlet") {
	if (comp == "x")
	  variable.fillFace(face,bc->getValue().x());
	if (comp == "y")
	  variable.fillFace(face,bc->getValue().y());
	if (comp == "z")
	  variable.fillFace(face,bc->getValue().z());
      }
      
      if (bc->getKind() == "Neumann") {
	if (comp == "x")
	  variable.fillFaceFlux(face,bc->getValue().x(),dx);
	if (comp == "y")
	  variable.fillFaceFlux(face,bc->getValue().y(),dx);
	if (comp == "z")
	  variable.fillFaceFlux(face,bc->getValue().z(),dx);
      }
    }
  }

}


/* ---------------------------------------------------------------------
 Function~  influx_outflux_volume--
 Purpose~   calculate the individual outfluxes and influxes for each cell.
            This includes the slabs and edge fluxes
 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden and 
    B.A. Kashiwa, Journal of Computational Physics, 146, 1-28, (1998) 
            
 Steps for each cell:  
 1) calculate the volume for each outflux
 3) set the influx_volume for the appropriate cell = to the q_outflux of the 
    adjacent cell. 

Implementation notes:
The outflux of volume is calculated in each cell in the computational domain
+ one layer of extra cells  surrounding the domain.The face-centered velocity 
needs to be defined on all faces for these cells 

01/02/01    Added corner flux terms and completed 3d edges

See schematic diagram at bottom of ice.cc for del* definitions
 ---------------------------------------------------------------------  */
void ICE::influxOutfluxVolume(const SFCXVariable<double>& uvel_FC,
			      const SFCYVariable<double>&     vvel_FC,
			      const SFCZVariable<double>&     wvel_FC,
			      const double&                   delT, 
			      const Patch*                    patch,
			      CCVariable<fflux>&              OFS, 
			      CCVariable<eflux>&              OFE,
			      CCVariable<cflux>&              OFC,
			      CCVariable<fflux>&              IFS, 
			      CCVariable<eflux>&              IFE,
			      CCVariable<cflux>&              IFC)
  
{
  Vector dx = patch->dCell();
  double delY_top, delY_bottom,delX_right, delX_left, delZ_front, delZ_back;
  double delX_tmp, delY_tmp,   delZ_tmp, totalfluxin;
  double vol = dx.x()*dx.y()*dx.z();

  // Compute outfluxes 
  for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); iter++){
    delY_top    = std::max(0.0, (vvel_FC[*iter+IntVector(0,1,0)] * delT));
    delY_bottom = std::max(0.0,-(vvel_FC[*iter+IntVector(0,0,0)] * delT));
    delX_right  = std::max(0.0, (uvel_FC[*iter+IntVector(1,0,0)] * delT));
    delX_left   = std::max(0.0,-(uvel_FC[*iter+IntVector(0,0,0)] * delT));
    delZ_front  = std::max(0.0, (wvel_FC[*iter+IntVector(0,0,1)] * delT));
    delZ_back   = std::max(0.0,-(wvel_FC[*iter+IntVector(0,0,0)] * delT));
    
    delX_tmp    = dx.x() - delX_right - delX_left;
    delY_tmp    = dx.y() - delY_top   - delY_bottom;
    delZ_tmp    = dx.z() - delZ_front - delZ_back;
    
    //__________________________________
    //   SLAB outfluxes
    OFS[*iter].d_fflux[TOP]    = delY_top     * delX_tmp * delZ_tmp;
    OFS[*iter].d_fflux[BOTTOM] = delY_bottom  * delX_tmp * delZ_tmp;
    OFS[*iter].d_fflux[RIGHT]  = delX_right   * delY_tmp * delZ_tmp;
    OFS[*iter].d_fflux[LEFT]   = delX_left    * delY_tmp * delZ_tmp;
    OFS[*iter].d_fflux[FRONT]  = delZ_front   * delX_tmp * delY_tmp;
    OFS[*iter].d_fflux[BACK]   = delZ_back    * delX_tmp * delY_tmp;
    //__________________________________
    // Edge flux terms
    OFE[*iter].d_eflux[TOP_R]     = delY_top      * delX_right * delZ_tmp;
    OFE[*iter].d_eflux[TOP_FR]    = delY_top      * delX_tmp   * delZ_front;
    OFE[*iter].d_eflux[TOP_L]     = delY_top      * delX_left  * delZ_tmp;
    OFE[*iter].d_eflux[TOP_BK]    = delY_top      * delX_tmp   * delZ_back;
    
    OFE[*iter].d_eflux[BOT_R]     = delY_bottom   * delX_right * delZ_tmp;
    OFE[*iter].d_eflux[BOT_FR]    = delY_bottom   * delX_tmp   * delZ_front;
    OFE[*iter].d_eflux[BOT_L]     = delY_bottom   * delX_left  * delZ_tmp;
    OFE[*iter].d_eflux[BOT_BK]    = delY_bottom   * delX_tmp   * delZ_back;
    
    OFE[*iter].d_eflux[RIGHT_BK]  = delY_tmp      * delX_right * delZ_back;
    OFE[*iter].d_eflux[RIGHT_FR]  = delY_tmp      * delX_right * delZ_front;
    
    OFE[*iter].d_eflux[LEFT_BK]   = delY_tmp      * delX_left  * delZ_back;
    OFE[*iter].d_eflux[LEFT_FR]   = delY_tmp      * delX_left  * delZ_front;
    
    //__________________________________
    //   Corner flux terms
    OFC[*iter].d_cflux[TOP_R_BK]  = delY_top      * delX_right * delZ_back;
    OFC[*iter].d_cflux[TOP_R_FR]  = delY_top      * delX_right * delZ_front;
    OFC[*iter].d_cflux[TOP_L_BK]  = delY_top      * delX_left  * delZ_back;
    OFC[*iter].d_cflux[TOP_L_FR]  = delY_top      * delX_left  * delZ_front;
    
    OFC[*iter].d_cflux[BOT_R_BK]  = delY_bottom   * delX_right * delZ_back;
    OFC[*iter].d_cflux[BOT_R_FR]  = delY_bottom   * delX_right * delZ_front;
    OFC[*iter].d_cflux[BOT_L_BK]  = delY_bottom   * delX_left  * delZ_back;
    OFC[*iter].d_cflux[BOT_L_FR]  = delY_bottom   * delX_left  * delZ_front;
  }
  //__________________________________
  //     INFLUX TERMS

  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
    IntVector curcell = *iter,adjcell;
    int i,j,k;
    //__________________________________
    //   INFLUX SLABS
    i = curcell.x();
    j = curcell.y();
    k = curcell.z();
    
    adjcell = IntVector(i, j+1, k);
    IFS[*iter].d_fflux[TOP]    = OFS[adjcell].d_fflux[BOTTOM];
    
    adjcell = IntVector(i, j-1, k);
    IFS[*iter].d_fflux[BOTTOM] = OFS[adjcell].d_fflux[TOP];
    
    adjcell = IntVector(i+1, j, k);
    IFS[*iter].d_fflux[RIGHT]  = OFS[adjcell].d_fflux[LEFT];
    
    adjcell = IntVector(i-1, j, k);
    IFS[*iter].d_fflux[LEFT]   = OFS[adjcell].d_fflux[RIGHT];
    
    adjcell = IntVector(i, j, k+1);
    IFS[*iter].d_fflux[FRONT]  = OFS[adjcell].d_fflux[BACK];
    
    adjcell = IntVector(i, j, k-1);
    IFS[*iter].d_fflux[BACK]   = OFS[adjcell].d_fflux[FRONT];
    
    //__________________________________
    //    INFLUX EDGES
    adjcell = IntVector(i+1, j+1, k);
    IFE[*iter].d_eflux[TOP_R]    = OFE[adjcell].d_eflux[BOT_L];
    
    adjcell = IntVector(i, j+1, k+1);
    IFE[*iter].d_eflux[TOP_FR]   = OFE[adjcell].d_eflux[BOT_BK];
    
    adjcell = IntVector(i-1, j+1, k);
    IFE[*iter].d_eflux[TOP_L]    = OFE[adjcell].d_eflux[BOT_R];
    
    adjcell = IntVector(i, j+1, k-1);
    IFE[*iter].d_eflux[TOP_BK]   = OFE[adjcell].d_eflux[BOT_FR];
    
    adjcell = IntVector(i+1, j-1, k);
    IFE[*iter].d_eflux[BOT_R]    = OFE[adjcell].d_eflux[TOP_L];
    
    adjcell = IntVector(i, j-1, k+1);
    IFE[*iter].d_eflux[BOT_FR]    = OFE[adjcell].d_eflux[TOP_BK];
    
    adjcell = IntVector(i-1, j-1, k);
    IFE[*iter].d_eflux[BOT_L]    = OFE[adjcell].d_eflux[TOP_R];
    
    adjcell = IntVector(i, j-1, k-1);
    IFE[*iter].d_eflux[BOT_BK]    = OFE[adjcell].d_eflux[TOP_FR];
    
    adjcell = IntVector(i+1, j, k-1);
    IFE[*iter].d_eflux[RIGHT_BK]  = OFE[adjcell].d_eflux[LEFT_FR];
    
    adjcell = IntVector(i+1, j, k+1);
    IFE[*iter].d_eflux[RIGHT_FR]  = OFE[adjcell].d_eflux[LEFT_BK];
    
    adjcell = IntVector(i-1, j, k-1);
    IFE[*iter].d_eflux[LEFT_BK]  = OFE[adjcell].d_eflux[RIGHT_FR];
    
    adjcell = IntVector(i-1, j, k+1);
    IFE[*iter].d_eflux[LEFT_FR]  = OFE[adjcell].d_eflux[RIGHT_BK];
    
    //__________________________________
    //   INFLUX CORNER FLUXES
    adjcell = IntVector(i+1, j+1, k-1);
    IFC[*iter].d_cflux[TOP_R_BK]= OFC[adjcell].d_cflux[BOT_L_FR];
    
    adjcell = IntVector(i+1, j+1, k+1);
    IFC[*iter].d_cflux[TOP_R_FR]= OFC[adjcell].d_cflux[BOT_L_BK];
    
    adjcell = IntVector(i-1, j+1, k-1);
    IFC[*iter].d_cflux[TOP_L_BK]= OFC[adjcell].d_cflux[BOT_R_FR];
    
    adjcell = IntVector(i-1, j+1, k+1);
    IFC[*iter].d_cflux[TOP_L_FR]= OFC[adjcell].d_cflux[BOT_R_BK];
    
    adjcell = IntVector(i+1, j-1, k-1);
    IFC[*iter].d_cflux[BOT_R_BK]= OFC[adjcell].d_cflux[TOP_L_FR];
    
    adjcell = IntVector(i+1, j-1, k+1);
    IFC[*iter].d_cflux[BOT_R_FR]= OFC[adjcell].d_cflux[TOP_L_BK];
    
    adjcell = IntVector(i-1, j-1, k-1);
    IFC[*iter].d_cflux[BOT_L_BK]= OFC[adjcell].d_cflux[TOP_R_FR];
    
    adjcell = IntVector(i-1, j-1, k+1);
    IFC[*iter].d_cflux[BOT_L_FR]= OFC[adjcell].d_cflux[TOP_R_BK];
    
    //__________________________________
    //  Bullet proofing
    totalfluxin = 0.0;
    for(int face = TOP; face <= BACK; face++ )  {
      totalfluxin  += IFS[*iter].d_fflux[face];
    }
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )  {
      totalfluxin  += IFE[*iter].d_eflux[edge];
    }
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      totalfluxin  += IFC[*iter].d_cflux[corner];
    }
  }
  ASSERT(totalfluxin < vol);
  }
  

/* ---------------------------------------------------------------------
 Function~  ICE::advectQFirst--ADVECTION:
 Purpose~   Calculate the advection of q_CC 
   
 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden and 
    B.A. Kashiwa, Journal of Computational Physics, 146, 1-28, (1998) 
            
 Steps for each cell:      
- Compute q outflux and q influx for each cell.
- Finally sum the influx and outflux portions
       
 01/02/01     Added corner flux terms and extened to 3d   

 advect_preprocessing MUST be done prior to this function
 ---------------------------------------------------------------------  */
void ICE::advectQFirst(const CCVariable<double>&   q_CC,const Patch* patch,
		       const CCVariable<fflux>&    OFS,
		       const CCVariable<eflux>&    OFE,
		       const CCVariable<cflux>&    OFC,
		       const CCVariable<fflux>&    IFS,
		       const CCVariable<eflux>&    IFE,
		       const CCVariable<cflux>&    IFC,
		       CCVariable<fflux>&          q_out,
		       CCVariable<eflux>&          q_out_EF,
		       CCVariable<cflux>&          q_out_CF,
		       CCVariable<fflux>&          q_in,
		       CCVariable<eflux>&          q_in_EF,
		       CCVariable<cflux>&          q_in_CF,
		       CCVariable<double>&         q_advected)
  
{
  double  sum_q_outflux, sum_q_outflux_EF, sum_q_outflux_CF, sum_q_influx;
  double sum_q_influx_EF, sum_q_influx_CF;

// Determine the influx and outflux of q at each cell
  qOutfluxFirst(q_CC, patch,q_out, q_out_EF,q_out_CF);
  
  qInflux(q_out, q_out_EF,q_out_CF, patch, q_in,q_in_EF,q_in_CF);
  
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {        
    sum_q_outflux       = 0.0;
    sum_q_outflux_EF    = 0.0;
    sum_q_outflux_CF    = 0.0;
    sum_q_influx        = 0.0;
    sum_q_influx_EF     = 0.0;
    sum_q_influx_CF     = 0.0;
    
    //__________________________________
    //  OUTFLUX: SLAB 
    for(int face = TOP; face <= BACK; face++ )  {
      sum_q_outflux  += q_out[*iter].d_fflux[face]         * 
	OFS[*iter].d_fflux[face];
    }
    //__________________________________
    //  OUTFLUX: EDGE_FLUX
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )   {
      sum_q_outflux_EF += q_out_EF[*iter].d_eflux[edge]    * 
	OFE[*iter].d_eflux[edge];
    }
    //__________________________________
    //  OUTFLUX: CORNER FLUX
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      sum_q_outflux_CF +=  q_out_CF[*iter].d_cflux[corner] * 
	OFC[*iter].d_cflux[corner];
    } 
    //__________________________________
    //  INFLUX: SLABS
    for(int face = TOP; face <= BACK; face++ )   {
      sum_q_influx  += q_in[*iter].d_fflux[face]           * 
	IFS[*iter].d_fflux[face];
    }
    //__________________________________
    //  INFLUX: EDGES
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )  {
      sum_q_influx_EF += q_in_EF[*iter].d_eflux[edge]      * 
	IFE[*iter].d_eflux[edge];
    }
    //__________________________________
    //   INFLUX: CORNER FLUX
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ ) {
      sum_q_influx_CF += q_in_CF[*iter].d_cflux[corner]    * 
	IFC[*iter].d_cflux[corner];
    }
    //__________________________________
    //  Calculate the advected q at t + delta t
    q_advected[*iter] = - sum_q_outflux - sum_q_outflux_EF - sum_q_outflux_CF
                        + sum_q_influx  + sum_q_influx_EF  + sum_q_influx_CF;

  }
  //__________________________________
  // DEBUGGING
 #if switch_Debug_advectQFirst
   for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
     cout<<*iter<< endl;
     for(int face = TOP; face <= BACK; face++ )  {
       fprintf(stderr, "slab: %i q_out %g, OFS %g\n",
	       face, q_out[*iter].d_fflux[face], OFS[*iter].d_fflux[face]);
     }
     for(int edge = TOP_R; edge <= LEFT_BK; edge++ )  {
       fprintf(stderr, "edge %i q_out_EF %g, OFE %g\n",
	       edge, q_out_EF[*iter].d_eflux[edge], OFE[*iter].d_eflux[edge]);
     }
     for(int face = TOP; face <= BACK; face++ )  {
       fprintf(stderr, "slab: %i q_in %g, IFS %g\n",
	       face, q_in[*iter].d_fflux[face], IFS[*iter].d_fflux[face]);
     }
     for(int edge = TOP_R; edge <= LEFT_BK; edge++ ) {
       fprintf(stderr, "edge: %i q_in_EF %g, IFE %g\n",
	       edge, q_in_EF[*iter].d_eflux[edge], IFE[*iter].d_eflux[edge]);
     }
   }
#endif

}

/*---------------------------------------------------------------------
 Function~  ICE::qOutfluxFirst-- 
 Purpose~  Calculate the quantity \langle q \rangle for each outflux, including
    the corner flux terms

 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden and 
    B.A. Kashiwa, Journal of Computational Physics, 
    146, 1-28, (1998) 
            
 Steps for each cell:  
 --------------------        
    Calculate the quantity outflux of q for each of the outflowing volumes 
       
01/02/01   Added corner fluxes
 
 See schematic diagram at bottom of ice.cc
 FIRST ORDER ONLY AT THIS TIME 10/21/00
---------------------------------------------------------------------  */ 
void ICE::qOutfluxFirst(const CCVariable<double>&   q_CC,const Patch* patch,
			CCVariable<fflux>& q_out, CCVariable<eflux>& q_out_EF,
			CCVariable<cflux>& q_out_CF)
{
  for(CellIterator iter = patch->getExtraCellIterator(); !iter.done(); iter++){
    //__________________________________
    //  SLABS
    for(int face = TOP; face <= BACK; face++ ) {
      q_out[*iter].d_fflux[face] = q_CC[*iter];
    }
    //__________________________________
    //  EDGE fluxes
    for(int edge = TOP_R; edge <= LEFT_BK; edge++ )  {
      q_out_EF[*iter].d_eflux[edge] = q_CC[*iter];
    }
    
    //__________________________________
    //  CORNER fluxes
    for(int corner = TOP_R_BK; corner <= BOT_L_FR; corner++ )  {
      q_out_CF[*iter].d_cflux[corner] = q_CC[*iter];
    }
  }
}

/*---------------------------------------------------------------------
 Function~  ICE::qInflux
 Purpose~
    Calculate the influx contribution \langle q \rangle for each slab and 
    corner flux.   
 
 References:
    "Compatible Fluxes for van Leer Advection" W.B VanderHeyden 
    and B.A. Kashiwa, Journal of Computational Physics, 146, 1-28, (1998) 
              
Implementation Notes:
    The quantity q_outflux is needed from one layer of extra cells surrounding
    the computational domain.
    
    Todd            01/02/01    Added corner cells and extend edged to 3d
See schematic diagram at bottom of file ice.cc
---------------------------------------------------------------------  */
void ICE::qInflux(const CCVariable<fflux>& q_out, 
		  const CCVariable<eflux>& q_out_EF, 
		  const CCVariable<cflux>& q_out_CF, const Patch* patch,
		  CCVariable<fflux>& q_in, CCVariable<eflux>& q_in_EF, 
		  CCVariable<cflux>& q_in_CF)

{
  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++) {
    IntVector curcell = *iter,adjcell;
    int i,j,k;
    
    //   INFLUX SLABS
    i = curcell.x();
    j = curcell.y();
    k = curcell.z();
    
    adjcell = IntVector(i, j+1, k);
    q_in[*iter].d_fflux[TOP]    = q_out[adjcell].d_fflux[BOTTOM];
    
    adjcell = IntVector(i, j-1, k);
    q_in[*iter].d_fflux[BOTTOM] = q_out[adjcell].d_fflux[TOP];
    
    adjcell = IntVector(i+1, j, k);
    q_in[*iter].d_fflux[RIGHT]  = q_out[adjcell].d_fflux[LEFT];
    
    adjcell = IntVector(i-1, j, k);
    q_in[*iter].d_fflux[LEFT]   = q_out[adjcell].d_fflux[RIGHT];
    
    adjcell = IntVector(i, j, k+1);
    q_in[*iter].d_fflux[FRONT]  = q_out[adjcell].d_fflux[BACK];
    
    adjcell = IntVector(i, j, k-1);
    q_in[*iter].d_fflux[BACK]   = q_out[adjcell].d_fflux[FRONT];
    
    //    INFLUX EDGES
    adjcell = IntVector(i+1, j+1, k);
    q_in_EF[*iter].d_eflux[TOP_R]    = q_out_EF[adjcell].d_eflux[BOT_L];
    
    adjcell = IntVector(i, j+1, k+1);
    q_in_EF[*iter].d_eflux[TOP_FR]   = q_out_EF[adjcell].d_eflux[BOT_BK];
    
    adjcell = IntVector(i-1, j+1, k);
    q_in_EF[*iter].d_eflux[TOP_L]    = q_out_EF[adjcell].d_eflux[BOT_R];
    
    adjcell = IntVector(i, j+1, k-1);
    q_in_EF[*iter].d_eflux[TOP_BK]   = q_out_EF[adjcell].d_eflux[BOT_FR];
    
    adjcell = IntVector(i+1, j-1, k);
    q_in_EF[*iter].d_eflux[BOT_R]    = q_out_EF[adjcell].d_eflux[TOP_L];
    
    adjcell = IntVector(i, j-1, k+1);
    q_in_EF[*iter].d_eflux[BOT_FR]    = q_out_EF[adjcell].d_eflux[TOP_BK];
    
    adjcell = IntVector(i-1, j-1, k);
    q_in_EF[*iter].d_eflux[BOT_L]    = q_out_EF[adjcell].d_eflux[TOP_R];
    
    adjcell = IntVector(i, j-1, k-1);
    q_in_EF[*iter].d_eflux[BOT_BK]    = q_out_EF[adjcell].d_eflux[TOP_FR];
    
    adjcell = IntVector(i+1, j, k-1);
    q_in_EF[*iter].d_eflux[RIGHT_BK]  = q_out_EF[adjcell].d_eflux[LEFT_FR];
    
    adjcell = IntVector(i+1, j, k+1);
    q_in_EF[*iter].d_eflux[RIGHT_FR]  = q_out_EF[adjcell].d_eflux[LEFT_BK];
    
    adjcell = IntVector(i-1, j, k-1);
    q_in_EF[*iter].d_eflux[LEFT_BK]  = q_out_EF[adjcell].d_eflux[RIGHT_FR];
    
    adjcell = IntVector(i-1, j, k+1);
    q_in_EF[*iter].d_eflux[LEFT_FR]  = q_out_EF[adjcell].d_eflux[RIGHT_BK];
    
    /*__________________________________
     *   INFLUX CORNER FLUXES
     *___________________________________*/
    adjcell = IntVector(i+1, j+1, k-1);
    q_in_CF[*iter].d_cflux[TOP_R_BK]= q_out_CF[adjcell].d_cflux[BOT_L_FR];
    
    adjcell = IntVector(i+1, j+1, k+1);
    q_in_CF[*iter].d_cflux[TOP_R_FR]= q_out_CF[adjcell].d_cflux[BOT_L_BK];
    
    adjcell = IntVector(i-1, j+1, k-1);
    q_in_CF[*iter].d_cflux[TOP_L_BK]= q_out_CF[adjcell].d_cflux[BOT_R_FR];
    
    adjcell = IntVector(i-1, j+1, k+1);
    q_in_CF[*iter].d_cflux[TOP_L_FR]= q_out_CF[adjcell].d_cflux[BOT_R_BK];
    
    adjcell = IntVector(i+1, j-1, k-1);
    q_in_CF[*iter].d_cflux[BOT_R_BK]= q_out_CF[adjcell].d_cflux[TOP_L_FR];
    
    adjcell = IntVector(i+1, j-1, k+1);
    q_in_CF[*iter].d_cflux[BOT_R_FR]= q_out_CF[adjcell].d_cflux[TOP_L_BK];
    
    adjcell = IntVector(i-1, j-1, k-1);
    q_in_CF[*iter].d_cflux[BOT_L_BK]= q_out_CF[adjcell].d_cflux[TOP_R_FR];
    
    adjcell = IntVector(i-1, j-1, k+1);
    q_in_CF[*iter].d_cflux[BOT_L_FR]= q_out_CF[adjcell].d_cflux[TOP_R_BK];
  }
}


/* 
 ======================================================================*
 Function:  printData--
 Purpose:  Print to stderr a cell-centered, single material
_______________________________________________________________________ */
void    ICE::printData(const Patch* patch, int include_GC,
        char    message1[],             /* message1                     */
        char    message2[],             /* message to user              */
        const CCVariable<double>& q_CC)
{
  int i, j, k,xLo, yLo, zLo, xHi, yHi, zHi;
  IntVector lowIndex, hiIndex; 
  
  fprintf(stderr,"______________________________________________\n");
  fprintf(stderr,"$%s\n",message1);
  fprintf(stderr,"$%s\n",message2);
  
  if (include_GC == 1)  { 
    lowIndex = patch->getCellLowIndex();
    hiIndex  = patch->getCellHighIndex();
  }
  if (include_GC == 0) {
    lowIndex = patch->getInteriorCellLowIndex();
    hiIndex  = patch->getInteriorCellHighIndex();
  }
  xLo = lowIndex.x();
  yLo = lowIndex.y();
  zLo = lowIndex.z();
  
  xHi = hiIndex.x();
  yHi = hiIndex.y();
  zHi = hiIndex.z();
  
  for(k = zLo; k < zHi; k++)  {
    for(j = yLo; j < yHi; j++) {
      for(i = xLo; i < xHi; i++) {
	IntVector idx(i, j, k);
	fprintf(stderr,"[%d,%d,%d]~ %15.14f  ",
		i,j,k, q_CC[idx]);
	
	/*  fprintf(stderr,"\n"); */
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr," ______________________________________________\n");
}

/* 
 ======================================================================*
 Function:  printData--
 Purpose:  Print to stderr a cell-centered, single material
_______________________________________________________________________ */
void    ICE::printData(const Patch* patch, int include_GC,
        char    message1[],             /* message1                     */
        char    message2[],             /* message to user              */
        const CCVariable<int>& q_CC)
{
  int i, j, k,xLo, yLo, zLo, xHi, yHi, zHi;
  IntVector lowIndex, hiIndex; 
  
  fprintf(stderr,"______________________________________________\n");
  fprintf(stderr,"$%s\n",message1);
  fprintf(stderr,"$%s\n",message2);
  
  if (include_GC == 1)  { 
    lowIndex = patch->getCellLowIndex();
    hiIndex  = patch->getCellHighIndex();
  }
  if (include_GC == 0) {
    lowIndex = patch->getInteriorCellLowIndex();
    hiIndex  = patch->getInteriorCellHighIndex();
  }
  xLo = lowIndex.x();
  yLo = lowIndex.y();
  zLo = lowIndex.z();
  
  xHi = hiIndex.x();
  yHi = hiIndex.y();
  zHi = hiIndex.z();
  
  for(k = zLo; k < zHi; k++)  {
    for(j = yLo; j < yHi; j++) {
      for(i = xLo; i < xHi; i++) {
	IntVector idx(i, j, k);
	fprintf(stderr,"[%d,%d,%d]~ %i  ",
		i,j,k, q_CC[idx]);
	
	/*  fprintf(stderr,"\n"); */
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr," ______________________________________________\n");
}
/* 
 ======================================================================*
 Function:  printVector--
 Purpose:  Print to stderr a cell-centered, single material
_______________________________________________________________________ */
void    ICE::printVector(const Patch* patch, int include_GC,
        char    message1[],             /* message1                     */
        char    message2[],             /* message to user              */
        int     component,              /*  x = 0,y = 1, z = 1          */
        const CCVariable<Vector>& q_CC)
{
  int i, j, k,xLo, yLo, zLo, xHi, yHi, zHi;
  IntVector lowIndex, hiIndex; 
  
  fprintf(stderr,"______________________________________________\n");
  fprintf(stderr,"$%s\n",message1);
  fprintf(stderr,"$%s\n",message2);
  
  if (include_GC == 1)  { 
    lowIndex = patch->getCellLowIndex();
    hiIndex  = patch->getCellHighIndex();
  }
  if (include_GC == 0) {
    lowIndex = patch->getInteriorCellLowIndex();
    hiIndex  = patch->getInteriorCellHighIndex();
  }
  xLo = lowIndex.x();
  yLo = lowIndex.y();
  zLo = lowIndex.z();
  
  xHi = hiIndex.x();
  yHi = hiIndex.y();
  zHi = hiIndex.z();
  
  for(k = zLo; k < zHi; k++)  {
    for(j = yLo; j < yHi; j++) {
      for(i = xLo; i < xHi; i++) {
	IntVector idx(i, j, k);
	fprintf(stderr,"[%d,%d,%d]~ %15.14f  ",
		i,j,k, q_CC[idx](component));
	
	/*  fprintf(stderr,"\n"); */
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr," ______________________________________________\n");
}


/* 
 ======================================================================*
 Function:  printData_FC--
 Purpose:  Print to stderr a cell-centered, single material
_______________________________________________________________________ */
void    ICE::printData_FC(const Patch* patch, int include_GC,
        char    message1[],             /* message1                     */
        char    message2[],             /* message to user              */
        const SFCXVariable<double>& q_FC)
{
  int i, j, k,xLo, yLo, zLo, xHi, yHi, zHi;
  IntVector lowIndex, hiIndex; 
  
  fprintf(stderr,"______________________________________________\n");
  fprintf(stderr,"$%s\n",message1);
  fprintf(stderr,"$%s\n",message2);
  
  if (include_GC == 1)  { 
    lowIndex = patch->getCellLowIndex();
    hiIndex  = patch->getCellHighIndex();
  }
  if (include_GC == 0) {
    lowIndex = patch->getInteriorCellLowIndex();
    hiIndex  = patch->getInteriorCellHighIndex();
  }
  xLo = lowIndex.x();
  yLo = lowIndex.y();
  zLo = lowIndex.z();
  
  xHi = hiIndex.x();
  yHi = hiIndex.y();
  zHi = hiIndex.z();
  
  for(k = zLo; k < zHi; k++)  {
    for(j = yLo; j < yHi; j++) {
      for(i = xLo; i < xHi; i++) {
	IntVector idx(i+1, j, k);
	fprintf(stderr,"[%d,%d,%d]~ %15.14f  ",
		i,j,k, q_FC[idx]);
	
	/*  fprintf(stderr,"\n"); */
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr," ______________________________________________\n");
}
/* 
 ======================================================================*
 Function:  printData_FC--
 Purpose:  Print to stderr a cell-centered, single material
_______________________________________________________________________ */
void    ICE::printData_FC(const Patch* patch, int include_GC,
        char    message1[],             /* message1                     */
        char    message2[],             /* message to user              */
        const SFCYVariable<double>& q_FC)
{
  int i, j, k,xLo, yLo, zLo, xHi, yHi, zHi;
  IntVector lowIndex, hiIndex; 
  
  fprintf(stderr,"______________________________________________\n");
  fprintf(stderr,"$%s\n",message1);
  fprintf(stderr,"$%s\n",message2);
  
  if (include_GC == 1)  { 
    lowIndex = patch->getCellLowIndex();
    hiIndex  = patch->getCellHighIndex();
  }
  if (include_GC == 0) {
    lowIndex = patch->getInteriorCellLowIndex();
    hiIndex  = patch->getInteriorCellHighIndex();
  }
  xLo = lowIndex.x();
  yLo = lowIndex.y();
  zLo = lowIndex.z();
  
  xHi = hiIndex.x();
  yHi = hiIndex.y();
  zHi = hiIndex.z();
  
  for(k = zLo; k < zHi; k++)  {
    for(j = yLo; j < yHi; j++) {
      for(i = xLo; i < xHi; i++) {
	IntVector idx(i,j+1, k);
	fprintf(stderr,"[%d,%d,%d]~ %15.14f  ",
		i,j,k, q_FC[idx]);
	
	/*  fprintf(stderr,"\n"); */
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr," ______________________________________________\n");
}

/* 
 ======================================================================*
 Function:  printData_FC--
 Purpose:  Print to stderr a cell-centered, single material
_______________________________________________________________________ */
void    ICE::printData_FC(const Patch* patch, int include_GC,
        char    message1[],             /* message1                     */
        char    message2[],             /* message to user              */
        const SFCZVariable<double>& q_FC)
{
  int i, j, k,xLo, yLo, zLo, xHi, yHi, zHi;
  IntVector lowIndex, hiIndex; 
  
  fprintf(stderr,"______________________________________________\n");
  fprintf(stderr,"$%s\n",message1);
  fprintf(stderr,"$%s\n",message2);
  
  if (include_GC == 1)  { 
    lowIndex = patch->getCellLowIndex();
    hiIndex  = patch->getCellHighIndex();
  }
  if (include_GC == 0) {
    lowIndex = patch->getInteriorCellLowIndex();
    hiIndex  = patch->getInteriorCellHighIndex();
  }
  xLo = lowIndex.x();
  yLo = lowIndex.y();
  zLo = lowIndex.z();
  
  xHi = hiIndex.x();
  yHi = hiIndex.y();
  zHi = hiIndex.z();
  
  for(k = zLo; k < zHi; k++)  {
    for(j = yLo; j < yHi; j++) {
      for(i = xLo; i < xHi; i++) {
	IntVector idx(i,j, k+1);
	fprintf(stderr,"[%d,%d,%d]~ %15.14f  ",
		i,j,k, q_FC[idx]);
	
	/*  fprintf(stderr,"\n"); */
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr," ______________________________________________\n");
}
/* 
 ======================================================================
 Function~  ICE::Message:
 Purpose~  Output an error message and stop the program if requested. 
 _______________________________________________________________________ */
void    ICE::Message(
        int     abort,          /* =1 then abort                            */                 
        char    message1[],   
        char    message2[],   
        char    message3[]) 
{        
  char    c[2];
  
  fprintf(stderr,"\n\n ______________________________________________\n");
  fprintf(stderr,"%s\n",message1);
  fprintf(stderr,"%s\n",message2);
  fprintf(stderr,"%s\n",message3);
  fprintf(stderr,"\n\n ______________________________________________\n");
  //______________________________
  // Now aborting program
  if(abort == 1) {
    fprintf(stderr,"\n");
    fprintf(stderr,"<c> = cvd  <d> = ddd\n");
    scanf("%s",c);
    system("date");
    if(strcmp(c, "c") == 0) system("cvd -P ice");
    if(strcmp(c, "d") == 0) system("ddd ice");
    exit(1); 
  }
}



#if 0
/*__________________________________
*   ONLY NEEDED BY SECOND ORDER ADVECTION
*___________________________________*/
void ICE::outflowVolCentroid(const SFCXVariable<double>& uvel_FC,
                             const SFCYVariable<double>& vvel_FC,
                             const SFCZVariable<double>& wvel_FC,
                             const double& delT, const Vector& dx,
                             CCVariable<fflux>& r_out_x,
                             CCVariable<fflux>& r_out_y,
                             CCVariable<fflux>& r_out_z,
                             CCVariable<eflux>& r_out_x_CF,
                             CCVariable<eflux>& r_out_y_CF,
                             CCVariable<eflux>& r_out_z_CF)

{

}

void ICE::qOutfluxSecond(CCVariable<fflux>& OFS,
                         CCVariable<fflux>& IFS,
                         CCVariable<fflux>& r_out_x,
                         CCVariable<fflux>& r_out_y,
                         CCVariable<fflux>& r_out_z,
                         CCVariable<eflux>& r_out_x_CF,
                         CCVariable<eflux>& r_out_y_CF,
                         CCVariable<eflux>& r_out_z_CF,
                         const Vector& dx)
{

}
#endif

#ifdef __sgi
#define IRIX
#pragma set woff 1209
#endif

namespace Uintah {

static MPI_Datatype makeMPI_fflux()
{
   ASSERTEQ(sizeof(ICE::fflux), sizeof(double)*6);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 6, 6, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ICE::fflux*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                               "ICE::fflux", true, &makeMPI_fflux);
   }
   return td;
}

static MPI_Datatype makeMPI_eflux()
{
   ASSERTEQ(sizeof(ICE::eflux), sizeof(double)*12);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 12, 12, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ICE::eflux*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                               "ICE::eflux", true, &makeMPI_eflux);
   }
   return td;
}

static MPI_Datatype makeMPI_cflux()
{
   ASSERTEQ(sizeof(ICE::cflux), sizeof(double)*8);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 8, 8, MPI_DOUBLE, &mpitype);
   MPI_Type_commit(&mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ICE::cflux*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
                               "ICE::cflux", true, &makeMPI_cflux);
   }
   return td;
}

} // end namespace Uintah
/*______________________________________________________________________
          S H E M A T I C   D I A G R A M S

                                    q_outflux(TOP)

                                        |    (I/O)flux_EF(TOP_BK)
                                        |
  (I/O)flux_CF(TOP_L_BK)       _________|___________
                              /___/_____|_______/__/|   (I/O)flux_CF(TOP_R_BK)
                             /   /      |      /  | |
                            /   /       |     /  /| |
  (I/O)flux_EF(TOP_L)      /   /             /  / |/|
                          /___/_____________/__/ ------ (I/O)flux_EF(TOP_R)
                        _/__ /_____________/__/| /| | 
                        |   |             |  | |/ | |   (I/O)flux_EF(BCK_R)
                        | + |      +      | +| /  | |      
                        |---|----------------|/|  |/| 
                        |   |             |  | | /| /  (I/O)flux_CF(BOT_R_BK)
  (I/O)flux(LEFT_FR)    | + |     i,j,k   | +| |/ /          
                        |   |             |  |/| /   (I/O)flux_EF(BOT_R)
                        |---|----------------| |/
  (I/O)flux_CF(BOT_L_FR)| + |      +      | +|/    (I/O)flux_CF(BOT_R_FR)
                        ---------------------- 
                         (I/O)flux_EF(BOT_FR)       
                         
                                         
                         
                            (TOP)      
   ______________________              ______________________  _
   |   |             |  |              |   |             |  |  |  delY_top
   | + |      +      | +|              | + |      +      | +|  |
   |---|----------------|  --ytop      |---|----------------|  -
   |   |             |  |              |   |             |  |
   | + |     i,j,k   | +| (RIGHT)      | + |     i,j,k   | +|
   |   |             |  |              |   |             |  |
   |---|----------------|  --y0        |---|----------------|  -
   | + |      +      | +|              | + |      +      | +|  | delY_bottom
   ----------------------              ----------------------  -
       |             |                 |---|             |--|
       x0            xright              delX_left         delX_right
       
                            (BACK)
   ______________________              ______________________  _
   |   |             |  |              |   |             |  |  |  delZ_back
   | + |      +      | +|              | + |      +      | +|  |
   |---|----------------|  --z0        |---|----------------|  -
   |   |             |  |              |   |             |  |
   | + |     i,j,k   | +| (RIGHT)      | + |     i,j,k   | +|
   |   |             |  |              |   |             |  |
   |---|----------------|  --z_frt     |---|----------------|  -
   | + |      +      | +|              | + |      +      | +|  | delZ_front
   ----------------------              ----------------------  -
       |             |                 |---|             |--|
       x0            xright              delX_left         delX_right
                         
______________________________________________________________________*/                        

