#include <Packages/Uintah/CCA/Components/MPM/SerialMPM.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/MPMLabel.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Packages/Uintah/CCA/Components/MPM/Contact/Contact.h>
#include <Packages/Uintah/CCA/Components/MPM/Contact/ContactFactory.h>
#include <Packages/Uintah/CCA/Components/MPM/ThermalContact/ThermalContact.h>
#include <Packages/Uintah/CCA/Components/MPM/ThermalContact/ThermalContactFactory.h>
#include <Packages/Uintah/CCA/Components/MPM/PhysicalBC/MPMPhysicalBCFactory.h>
#include <Packages/Uintah/CCA/Components/MPM/PhysicalBC/ForceBC.h>
#include <Packages/Uintah/Core/Math/Matrix3.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/CCVariable.h>
#include <Packages/Uintah/Core/Grid/NCVariable.h>
#include <Packages/Uintah/Core/Grid/ParticleSet.h>
#include <Packages/Uintah/Core/Grid/ParticleVariable.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Grid/NodeIterator.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/SoleVariable.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/BoundCond.h>
#include <Packages/Uintah/Core/Grid/VelocityBoundCond.h>
#include <Packages/Uintah/Core/Grid/SymmetryBoundCond.h>
#include <Packages/Uintah/Core/Grid/TemperatureBoundCond.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/Core/Exceptions/ParameterNotFound.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/Core/Grid/fillFace.h>

#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MinMax.h>
#include <Core/Util/NotFinished.h>
#include <Core/Util/DebugStream.h>

#include <iostream>
#include <fstream>

#undef KUMAR
//#define KUMAR

using namespace Uintah;
using namespace SCIRun;

using namespace std;

#define MAX_BASIS 27
#undef INTEGRAL_TRACTION

static DebugStream cout_doing("MPM", false);

// From ThreadPool.cc:  Used for syncing cerr'ing so it is easier to read.
extern Mutex cerrLock;

SerialMPM::SerialMPM(const ProcessorGroup* myworld) :
  UintahParallelComponent(myworld)
{
  lb = scinew MPMLabel();
  d_nextOutputTime=0.;
  d_SMALL_NUM_MPM=1e-200;
  d_with_ice    = false;
  d_with_arches = false;
  contactModel        = 0;
  thermalContactModel = 0;
  d_8or27 = 8;
  d_min_part_mass = 3.e-15;
  d_max_vel = 3.e105;
  d_artificial_viscosity = false;
  NGP     = 1;
  NGN     = 1;

  d_artificialDampCoeff = 0.0;
  d_dampingRateLabel = 
    VarLabel::create("dampingRate", sum_vartype::getTypeDescription() );
  d_dampingCoeffLabel = 
    VarLabel::create("dampingCoeff", max_vartype::getTypeDescription() );

}

SerialMPM::~SerialMPM()
{
  delete lb;
  delete contactModel;
  delete thermalContactModel;
  MPMPhysicalBCFactory::clean();
  VarLabel::destroy(d_dampingRateLabel);
  VarLabel::destroy(d_dampingCoeffLabel);
}

void SerialMPM::problemSetup(const ProblemSpecP& prob_spec, GridP& /*grid*/,
			     SimulationStateP& sharedState)
{
   d_sharedState = sharedState;

   ProblemSpecP mpm_soln_ps = prob_spec->findBlock("MPM");

   if(mpm_soln_ps) {
     mpm_soln_ps->get("nodes8or27", d_8or27);
     mpm_soln_ps->get("minimum_particle_mass",    d_min_part_mass);
     mpm_soln_ps->get("maximum_particle_melocity",d_max_vel);
     mpm_soln_ps->get("artificial_damping_coeff", d_artificialDampCoeff);
     mpm_soln_ps->get("artificial_viscosity",     d_artificial_viscosity);
   }

   if(d_8or27==8){
     NGP=1;
     NGN=1;
   } else if(d_8or27==MAX_BASIS){
     NGP=2;
     NGN=2;
   }

  //__________________________________
  // Grab time_integrator, default is explicit
   string integrator_type = "explicit";
   d_integrator = Explicit;
   if (mpm_soln_ps ) {
     mpm_soln_ps->get("time_integrator",integrator_type);
     if (integrator_type == "implicit"){
       d_integrator = Implicit;
     }
     if (integrator_type == "explicit") {
       d_integrator = Explicit;
     }
   }
   
   MPMPhysicalBCFactory::create(prob_spec);

   contactModel = ContactFactory::create(prob_spec,sharedState, lb, d_8or27);
   thermalContactModel =
		 ThermalContactFactory::create(prob_spec, sharedState, lb);

   ProblemSpecP p = prob_spec->findBlock("DataArchiver");
   if(!p->get("outputInterval", d_outputInterval))
      d_outputInterval = 1.0;

   //Search for the MaterialProperties block and then get the MPM section

   ProblemSpecP mat_ps =  prob_spec->findBlock("MaterialProperties");

   ProblemSpecP mpm_mat_ps = mat_ps->findBlock("MPM");

   for (ProblemSpecP ps = mpm_mat_ps->findBlock("material"); ps != 0;
       ps = ps->findNextBlock("material") ) {
     MPMMaterial *mat = scinew MPMMaterial(ps, lb, d_8or27,integrator_type);
     //register as an MPM material
     sharedState->registerMPMMaterial(mat);
   }

}

void SerialMPM::scheduleInitialize(const LevelP& level,
				   SchedulerP& sched)
{
  Task* t = scinew Task("SerialMPM::actuallyInitialize",
			this, &SerialMPM::actuallyInitialize);

  MaterialSubset* zeroth_matl = scinew MaterialSubset();
  zeroth_matl->add(0);
  zeroth_matl->addReference();

  t->computes(lb->partCountLabel);
  t->computes(lb->pXLabel);
  t->computes(lb->pMassLabel);
  t->computes(lb->pVolumeLabel);
  t->computes(lb->pTemperatureLabel);
  t->computes(lb->pVelocityLabel);
  t->computes(lb->pExternalForceLabel);
  t->computes(lb->pParticleIDLabel);
  t->computes(lb->pDeformationMeasureLabel);
  t->computes(lb->pStressLabel);
  t->computes(lb->pSizeLabel);
  t->computes(d_sharedState->get_delt_label());
  t->computes(lb->pCellNAPIDLabel,zeroth_matl);

  // Compute the accumulated strain energy
  t->computes(lb->AccStrainEnergyLabel);

  // artificial damping coeff initialized to 0.0
  if (d_artificialDampCoeff > 0.0) {
     t->computes(d_dampingRateLabel); 
     t->computes(d_dampingCoeffLabel); 
  }

  int numMPM = d_sharedState->getNumMPMMatls();
  const PatchSet* patches = level->eachPatch();
  for(int m = 0; m < numMPM; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addInitialComputesAndRequires(t, mpm_matl, patches);
  }

  sched->addTask(t, level->eachPatch(), d_sharedState->allMPMMaterials());

  t = scinew Task("SerialMPM::printParticleCount",
		  this, &SerialMPM::printParticleCount);
  t->requires(Task::NewDW, lb->partCountLabel);
  sched->addTask(t, level->eachPatch(), d_sharedState->allMPMMaterials());

  // The task will have a reference to zeroth_matl
  if (zeroth_matl->removeReference())
    delete zeroth_matl; // shouln't happen, but...
}

void SerialMPM::scheduleComputeStableTimestep(const LevelP&,
					      SchedulerP&)
{
   // Nothing to do here - delt is computed as a by-product of the
   // consitutive model
}

void
SerialMPM::scheduleTimeAdvance(const LevelP & level,
			       SchedulerP   & sched,
			       int, int ) // AMR Parameters
{
  const PatchSet* patches = level->eachPatch();
  const MaterialSet* matls = d_sharedState->allMPMMaterials();

  scheduleInterpolateParticlesToGrid(     sched, patches, matls);
  scheduleComputeHeatExchange(            sched, patches, matls);
  scheduleExMomInterpolated(              sched, patches, matls);
  scheduleComputeStressTensor(            sched, patches, matls);
  scheduleComputeInternalForce(           sched, patches, matls);
  scheduleComputeInternalHeatRate(        sched, patches, matls);
  scheduleSolveEquationsMotion(           sched, patches, matls);
  scheduleSolveHeatEquations(             sched, patches, matls);
  scheduleIntegrateAcceleration(          sched, patches, matls);
  // scheduleIntegrateTemperatureRate(    sched, patches, matls);
  scheduleExMomIntegrated(                sched, patches, matls);
  scheduleSetGridBoundaryConditions(      sched, patches, matls);
  scheduleApplyExternalLoads(             sched, patches, matls);
  scheduleCalculateDampingRate(           sched, patches, matls);
  scheduleInterpolateToParticlesAndUpdate(sched, patches, matls);

  sched->scheduleParticleRelocation(level, lb->pXLabel_preReloc,
				    lb->d_particleState_preReloc,
				    lb->pXLabel, lb->d_particleState,
				    lb->pParticleIDLabel, matls);
}

void SerialMPM::scheduleInterpolateParticlesToGrid(SchedulerP& sched,
						   const PatchSet* patches,
						   const MaterialSet* matls)
{
  /* interpolateParticlesToGrid
   *   in(P.MASS, P.VELOCITY, P.NAT_X)
   *   operation(interpolate the P.MASS and P.VEL to the grid
   *             using P.NAT_X and some shape function evaluations)
   *   out(G.MASS, G.VELOCITY) */


  Task* t = scinew Task("SerialMPM::interpolateParticlesToGrid",
			this,&SerialMPM::interpolateParticlesToGrid);
  Ghost::GhostType  gan = Ghost::AroundNodes;
  t->requires(Task::OldDW, lb->pMassLabel,             gan,NGP);
  t->requires(Task::OldDW, lb->pVolumeLabel,           gan,NGP);
  
  t->requires(Task::OldDW, lb->pVelocityLabel,         gan,NGP);
  t->requires(Task::OldDW, lb->pXLabel,                gan,NGP);
  t->requires(Task::OldDW, lb->pExternalForceLabel,    gan,NGP);
  t->requires(Task::OldDW, lb->pTemperatureLabel,      gan,NGP);
  t->requires(Task::OldDW, lb->pSp_volLabel,           gan,NGP); 
  
  if(d_8or27==27){
   t->requires(Task::OldDW,lb->pSizeLabel,             gan,NGP);
  }
//t->requires(Task::OldDW, lb->pExternalHeatRateLabel, gan,NGP);

  t->computes(lb->gMassLabel);
  t->computes(lb->gMassLabel,        d_sharedState->getAllInOneMatl(),
	      Task::OutOfDomain);
  t->computes(lb->gTemperatureLabel, d_sharedState->getAllInOneMatl(),
	      Task::OutOfDomain);
  t->computes(lb->gVelocityLabel,    d_sharedState->getAllInOneMatl(),
	      Task::OutOfDomain);
  t->computes(lb->gSp_volLabel);
  t->computes(lb->gVolumeLabel);
  t->computes(lb->gVelocityLabel);
  t->computes(lb->gExternalForceLabel);
  t->computes(lb->gTemperatureLabel);
  t->computes(lb->gTemperatureNoBCLabel);
  t->computes(lb->gExternalHeatRateLabel);
  t->computes(lb->gNumNearParticlesLabel);
  t->computes(lb->TotalMassLabel);
  

  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleComputeHeatExchange(SchedulerP& sched,
					    const PatchSet* patches,
					    const MaterialSet* matls)
{
 /* computeHeatExchange
  *   in(G.MASS, G.TEMPERATURE, G.EXTERNAL_HEAT_RATE)
  *   operation(peform heat exchange which will cause each of
  *   velocity fields to exchange heat according to 
  *   the temperature differences)
  *   out(G.EXTERNAL_HEAT_RATE) */


  Task* t = scinew Task("ThermalContact::computeHeatExchange",
		        thermalContactModel,
		        &ThermalContact::computeHeatExchange);

  thermalContactModel->addComputesAndRequires(t, patches, matls);
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleExMomInterpolated(SchedulerP& sched,
					  const PatchSet* patches,
					  const MaterialSet* matls)
{
  Task* t = scinew Task("Contact::exMomInterpolated",
		    contactModel,
		    &Contact::exMomInterpolated);

  contactModel->addComputesAndRequiresInterpolated(t, patches, matls);
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleComputeStressTensor(SchedulerP& sched,
					    const PatchSet* patches,
					    const MaterialSet* matls)
{
  int numMatls = d_sharedState->getNumMPMMatls();
  Task* t = scinew Task("SerialMPM::computeStressTensor",
		    this, &SerialMPM::computeStressTensor);
  for(int m = 0; m < numMatls; m++){
    MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
    ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
    cm->addComputesAndRequires(t, mpm_matl, patches);
  }
	 
  t->computes(d_sharedState->get_delt_label());
  t->computes(lb->StrainEnergyLabel);

  sched->addTask(t, patches, matls);

  // Compute the accumulated strain energy
  t = scinew Task("SerialMPM::computeAccStrainEnergy",
		  this, &SerialMPM::computeAccStrainEnergy);
  t->requires(Task::OldDW, lb->AccStrainEnergyLabel);
  t->requires(Task::NewDW, lb->StrainEnergyLabel);
  t->computes(lb->AccStrainEnergyLabel);
  sched->addTask(t, patches, matls);

  if(d_artificial_viscosity){
    scheduleComputeArtificialViscosity(   sched, patches, matls);
  }
}

void SerialMPM::scheduleComputeArtificialViscosity(SchedulerP& sched,
                                                   const PatchSet* patches,
                                                   const MaterialSet* matls)
{
  Task* t = scinew Task("SerialMPM::computeArtificialViscosity",
		    this, &SerialMPM::computeArtificialViscosity);

  Ghost::GhostType  gac = Ghost::AroundCells;
  t->requires(Task::OldDW, lb->pXLabel,                 Ghost::None);
  t->requires(Task::OldDW, lb->pMassLabel,              Ghost::None);
  t->requires(Task::NewDW, lb->pVolumeDeformedLabel,    Ghost::None);

  if(d_8or27==27){
    t->requires(Task::OldDW,lb->pSizeLabel,             Ghost::None);
  }

  t->requires(Task::NewDW,lb->gVelocityLabel, gac, NGN);
  t->computes(lb->p_qLabel);

  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleComputeInternalForce(SchedulerP& sched,
					     const PatchSet* patches,
					     const MaterialSet* matls)
{
 /*
  * computeInternalForce
  *   in(P.CONMOD, P.NAT_X, P.VOLUME)
  *   operation(evaluate the divergence of the stress (stored in
  *   P.CONMOD) using P.NAT_X and the gradients of the
  *   shape functions)
  * out(G.F_INTERNAL) */

  Task* t = scinew Task("SerialMPM::computeInternalForce",
		    this, &SerialMPM::computeInternalForce);

  Ghost::GhostType  gan   = Ghost::AroundNodes;
  Ghost::GhostType  gnone = Ghost::None;
  t->requires(Task::NewDW,lb->gMassLabel, gnone);
  t->requires(Task::NewDW,lb->gMassLabel, d_sharedState->getAllInOneMatl(),
						Task::OutOfDomain, gnone);
  t->requires(Task::NewDW,lb->pStressLabel_preReloc,      gan,NGP);
  t->requires(Task::NewDW,lb->pVolumeDeformedLabel,       gan,NGP);
  t->requires(Task::OldDW,lb->pXLabel,                    gan,NGP);
  t->requires(Task::OldDW,lb->pMassLabel,                 gan,NGP);
  if(d_8or27==27){
   t->requires(Task::OldDW, lb->pSizeLabel,               gan,NGP);
  }

  if(d_with_ice){
    t->requires(Task::NewDW, lb->pPressureLabel,          gan,NGP);
  }
  if(d_artificial_viscosity){
    t->requires(Task::NewDW, lb->p_qLabel,                gan,NGP);
  }

  t->computes(lb->gInternalForceLabel);
#ifdef INTEGRAL_TRACTION
  t->computes(lb->NTractionZMinusLabel);
#endif
  t->computes(lb->gStressForSavingLabel);
  t->computes(lb->gStressForSavingLabel, d_sharedState->getAllInOneMatl(),
	      Task::OutOfDomain);

  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleComputeInternalHeatRate(SchedulerP& sched,
						const PatchSet* patches,
						const MaterialSet* matls)
{  
  /*
   * computeInternalHeatRate
   * out(G.INTERNALHEATRATE) */

  Task* t = scinew Task("SerialMPM::computeInternalHeatRate",
			this, &SerialMPM::computeInternalHeatRate);

  Ghost::GhostType  gan = Ghost::AroundNodes;
  Ghost::GhostType  gac = Ghost::AroundCells;
  t->requires(Task::OldDW, lb->pXLabel,              gan, NGP);
  if(d_8or27==27){
   t->requires(Task::OldDW, lb->pSizeLabel,          gan, NGP);
  }
  t->requires(Task::NewDW, lb->pVolumeDeformedLabel, gan, NGP);
  t->requires(Task::NewDW, lb->gTemperatureLabel,    gac, 2*NGP);

  t->computes(lb->gInternalHeatRateLabel);
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleSolveEquationsMotion(SchedulerP& sched,
					     const PatchSet* patches,
					     const MaterialSet* matls)
{
  /* solveEquationsMotion
   *   in(G.MASS, G.F_INTERNAL)
   *   operation(acceleration = f/m)
   *   out(G.ACCELERATION) */

  Task* t = scinew Task("SerialMPM::solveEquationsMotion",
		    this, &SerialMPM::solveEquationsMotion);

  t->requires(Task::OldDW, d_sharedState->get_delt_label());

  t->requires(Task::NewDW, lb->gMassLabel,          Ghost::None);
  t->requires(Task::NewDW, lb->gInternalForceLabel, Ghost::None);
  t->requires(Task::NewDW, lb->gExternalForceLabel, Ghost::None);
//Uncomment  the next line to use damping
//t->requires(Task::NewDW, lb->gVelocityLabel,      Ghost::None);     

  if(d_with_ice){
    t->requires(Task::NewDW, lb->gradPAccNCLabel,   Ghost::None);
  }
  if(d_with_arches){
    t->requires(Task::NewDW, lb->AccArchesNCLabel,  Ghost::None);
  }

  t->computes(lb->gAccelerationLabel);
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleSolveHeatEquations(SchedulerP& sched,
					   const PatchSet* patches,
					   const MaterialSet* matls)
{
  /* solveHeatEquations
   *   in(G.MASS, G.INTERNALHEATRATE, G.EXTERNALHEATRATE)
   *   out(G.TEMPERATURERATE) */

  Task* t = scinew Task("SerialMPM::solveHeatEquations",
			    this, &SerialMPM::solveHeatEquations);

  Ghost::GhostType  gnone = Ghost::None;
  t->requires(Task::NewDW, lb->gMassLabel,                           gnone);
  t->requires(Task::NewDW, lb->gVolumeLabel,                         gnone);
  t->requires(Task::NewDW, lb->gExternalHeatRateLabel,               gnone);
  t->requires(Task::NewDW, lb->gInternalHeatRateLabel,               gnone);
  t->requires(Task::NewDW, lb->gThermalContactHeatExchangeRateLabel, gnone);
		
  if(d_with_arches){
    t->requires(Task::NewDW, lb->heaTranSolid_NCLabel, gnone);
  }
		
  t->computes(lb->gTemperatureRateLabel);

  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleIntegrateAcceleration(SchedulerP& sched,
					      const PatchSet* patches,
					      const MaterialSet* matls)
{
  /* integrateAcceleration
   *   in(G.ACCELERATION, G.VELOCITY)
   *   operation(v* = v + a*dt)
   *   out(G.VELOCITY_STAR) */

  Task* t = scinew Task("SerialMPM::integrateAcceleration",
			    this, &SerialMPM::integrateAcceleration);

  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->requires(Task::NewDW, lb->gAccelerationLabel,      Ghost::None);
  t->requires(Task::NewDW, lb->gVelocityLabel,          Ghost::None);

  t->computes(lb->gVelocityStarLabel);

  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleIntegrateTemperatureRate(SchedulerP& sched,
						 const PatchSet* patches,
						 const MaterialSet* matls)
{
  /* integrateTemperatureRate
   *   in(G.TEMPERATURE, G.TEMPERATURERATE)
   *   operation(t* = t + t_rate * dt)
   *   out(G.TEMPERATURE_STAR) */

  Task* t = scinew Task("SerialMPM::integrateTemperatureRate",
		    this, &SerialMPM::integrateTemperatureRate);

  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  t->requires(Task::NewDW, lb->gTemperatureLabel,     Ghost::None);
  t->requires(Task::NewDW, lb->gTemperatureRateLabel, Ghost::None);
		     
  t->computes(lb->gTemperatureStarLabel);
		     
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleExMomIntegrated(SchedulerP& sched,
					const PatchSet* patches,
					const MaterialSet* matls)
{
  /* exMomIntegrated
   *   in(G.MASS, G.VELOCITY_STAR, G.ACCELERATION)
   *   operation(peform operations which will cause each of
   *		  velocity fields to feel the influence of the
   *		  the others according to specific rules)
   *   out(G.VELOCITY_STAR, G.ACCELERATION) */

  Task* t = scinew Task("Contact::exMomIntegrated",
		   contactModel,
		   &Contact::exMomIntegrated);

  contactModel->addComputesAndRequiresIntegrated(t, patches, matls);
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleSetGridBoundaryConditions(SchedulerP& sched,
						       const PatchSet* patches,
						       const MaterialSet* matls)

{
  Task* t=scinew Task("SerialMPM::setGridBoundaryConditions",
		    this, &SerialMPM::setGridBoundaryConditions);
                  
  const MaterialSubset* mss = matls->getUnion();
  t->requires(Task::OldDW, d_sharedState->get_delt_label() );
  
  t->modifies(             lb->gAccelerationLabel,     mss);
  t->modifies(             lb->gVelocityStarLabel,     mss);
  t->modifies(             lb->gTemperatureRateLabel,  mss);
  t->requires(Task::NewDW, lb->gTemperatureNoBCLabel,  Ghost::None,0);
  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleApplyExternalLoads(SchedulerP& sched,
                                           const PatchSet* patches,
                                           const MaterialSet* matls)

{
 /*
  * applyExternalLoads
  *   in(p.externalForce, p.externalheatrate)
  *   out(p.externalForceNew, p.externalheatrateNew) */
  Task* t=scinew Task("SerialMPM::applyExternalLoads",
		    this, &SerialMPM::applyExternalLoads);
                  
  // BB : 11/09/02 Unused variable
  //const MaterialSubset* mss = matls->getUnion();
  
  t->requires(Task::OldDW, lb->pExternalForceLabel,    Ghost::None);
  t->computes(             lb->pExtForceLabel_preReloc);

//  t->computes(Task::OldDW, lb->pExternalHeatRateLabel_preReloc);

  sched->addTask(t, patches, matls);
}

void SerialMPM::scheduleCalculateDampingRate(SchedulerP& sched,
					     const PatchSet* patches,
					     const MaterialSet* matls)
{
 /*
  * calculateDampingRate
  *   in(G.VELOCITY_STAR, P.X, P.Size)
  *   operation(Calculate the interpolated particle velocity and
  *             sum the squares of the velocities over particles)
  *   out(sum_vartpe(dampingRate)) 
  */
  if (d_artificialDampCoeff > 0.0) {
    Task* t=scinew Task("SerialMPM::calculateDampingRate", this, 
			&SerialMPM::calculateDampingRate);
    t->requires(Task::NewDW, lb->gVelocityStarLabel, Ghost::AroundCells, NGN);
    t->requires(Task::OldDW, lb->pXLabel, Ghost::None);
    if(d_8or27==27) t->requires(Task::OldDW, lb->pSizeLabel, Ghost::None);

    t->computes(d_dampingRateLabel);
    sched->addTask(t, patches, matls);
  }
}

void SerialMPM::scheduleInterpolateToParticlesAndUpdate(SchedulerP& sched,
						       const PatchSet* patches,
						       const MaterialSet* matls)

{
 /*
  * interpolateToParticlesAndUpdate
  *   in(G.ACCELERATION, G.VELOCITY_STAR, P.NAT_X)
  *   operation(interpolate acceleration and v* to particles and
  *   integrate these to get new particle velocity and position)
  * out(P.VELOCITY, P.X, P.NAT_X) */

  Task* t=scinew Task("SerialMPM::interpolateToParticlesAndUpdate",
		    this, &SerialMPM::interpolateToParticlesAndUpdate);


  t->requires(Task::OldDW, d_sharedState->get_delt_label() );

  Ghost::GhostType  gac = Ghost::AroundCells;
  t->requires(Task::NewDW, lb->gAccelerationLabel,     gac,NGN);
  t->requires(Task::NewDW, lb->gVelocityStarLabel,     gac,NGN);
  t->requires(Task::NewDW, lb->gTemperatureRateLabel,  gac,NGN);
  t->requires(Task::NewDW, lb->gTemperatureLabel,      gac,NGN);
  t->requires(Task::NewDW, lb->gTemperatureNoBCLabel,  gac,NGN);
  t->requires(Task::NewDW, lb->gSp_volLabel,           gac,NGN);
  t->requires(Task::NewDW, lb->frictionalWorkLabel,    gac,NGN);
  t->requires(Task::OldDW, lb->pXLabel,                Ghost::None);
  t->requires(Task::OldDW, lb->pMassLabel,             Ghost::None);
  t->requires(Task::OldDW, lb->pParticleIDLabel,       Ghost::None);
  t->requires(Task::OldDW, lb->pTemperatureLabel,      Ghost::None);
  t->requires(Task::OldDW, lb->pSp_volLabel,           Ghost::None); 
  t->requires(Task::OldDW, lb->pVelocityLabel,         Ghost::None);
  t->requires(Task::OldDW, lb->pMassLabel,             Ghost::None);
  if(d_8or27==27){
   t->requires(Task::OldDW, lb->pSizeLabel,            Ghost::None);
  }
  t->requires(Task::NewDW, lb->pVolumeDeformedLabel,   Ghost::None);

  // The dampingCoeff (alpha) is 0.0 for standard usage, otherwise
  // it is determined by the damping rate if the artificial damping
  // coefficient Q is greater than 0.0
  if (d_artificialDampCoeff > 0.0) {
    t->requires(Task::OldDW, d_dampingCoeffLabel);
    t->requires(Task::NewDW, d_dampingRateLabel);
    t->computes(d_dampingCoeffLabel);
  }

  if(d_with_ice){
    t->requires(Task::NewDW, lb->dTdt_NCLabel,         gac,NGN);
    t->requires(Task::NewDW, lb->gSp_vol_srcLabel,     gac,NGN);
    t->requires(Task::NewDW, lb->massBurnFractionLabel,gac,NGN);
  }

  t->computes(lb->pVelocityLabel_preReloc);
  t->computes(lb->pXLabel_preReloc);
  t->computes(lb->pParticleIDLabel_preReloc);
  t->computes(lb->pTemperatureLabel_preReloc);
  t->computes(lb->pSp_volLabel_preReloc);
  t->computes(lb->pMassLabel_preReloc);
  t->computes(lb->pVolumeLabel_preReloc);
  if(d_8or27==27){
    t->computes(lb->pSizeLabel_preReloc);
  }

  t->computes(lb->KineticEnergyLabel);
  t->computes(lb->CenterOfMassPositionLabel);
  t->computes(lb->CenterOfMassVelocityLabel);
  sched->addTask(t, patches, matls);
}

void SerialMPM::printParticleCount(const ProcessorGroup* pg,
				   const PatchSubset*,
				   const MaterialSubset*,
				   DataWarehouse*,
				   DataWarehouse* new_dw)
{
  sumlong_vartype pcount;
  new_dw->get(pcount, lb->partCountLabel);
  
  if(pg->myrank() == 0){
    static bool printed=false;
    if(!printed){
      cerr << "Created " << (long) pcount << " total particles\n";
      printed=true;
    }
  }
}

void SerialMPM::computeAccStrainEnergy(const ProcessorGroup*,
				       const PatchSubset*,
				       const MaterialSubset*,
				       DataWarehouse* old_dw,
				       DataWarehouse* new_dw)
{
  // Get the totalStrainEnergy from the old datawarehouse
  max_vartype accStrainEnergy;
  old_dw->get(accStrainEnergy, lb->AccStrainEnergyLabel);
	
  // Get the incremental strain energy from the new datawarehouse
  sum_vartype incStrainEnergy;
  new_dw->get(incStrainEnergy, lb->StrainEnergyLabel);
	                  
  // Add the two a put into new dw
  double totalStrainEnergy = 
    (double) accStrainEnergy + (double) incStrainEnergy;
  new_dw->put(max_vartype(totalStrainEnergy), lb->AccStrainEnergyLabel);
}

void SerialMPM::actuallyInitialize(const ProcessorGroup*,
				   const PatchSubset* patches,
				   const MaterialSubset* matls,
				   DataWarehouse*,
				   DataWarehouse* new_dw)
{
  particleIndex totalParticles=0;
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing actuallyInitialize on patch " << patch->getID()
	       <<"\t\t\t MPM"<< endl;

    CCVariable<short int> cellNAPID;
    new_dw->allocateAndPut(cellNAPID, lb->pCellNAPIDLabel, 0, patch);
    cellNAPID.initialize(0);

    for(int m=0;m<matls->size();m++){
      //cerrLock.lock();
      //NOT_FINISHED("not quite right - mapping of matls, use matls->get()");
      //cerrLock.unlock();
       MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
       particleIndex numParticles = mpm_matl->countParticles(patch);
       totalParticles+=numParticles;

       mpm_matl->createParticles(numParticles, cellNAPID, patch, new_dw);

       mpm_matl->getConstitutiveModel()->initializeCMData(patch,
						mpm_matl, new_dw);
    }
  }

  // Initialize the accumulated strain energy
  new_dw->put(max_vartype(0.0), lb->AccStrainEnergyLabel);
  
  // Initialize the artificial damping ceofficient (alpha) to zero
  if (d_artificialDampCoeff > 0.0) {
    double alpha = 0.0;    
    double alphaDot = 0.0;    
    new_dw->put(max_vartype(alpha), d_dampingCoeffLabel);
    new_dw->put(sum_vartype(alphaDot), d_dampingRateLabel);
  }

  new_dw->put(sumlong_vartype(totalParticles), lb->partCountLabel);
}


void SerialMPM::actuallyComputeStableTimestep(const ProcessorGroup*,
					      const PatchSubset*,
					      const MaterialSubset*,
					      DataWarehouse*,
					      DataWarehouse*)
{
}

void SerialMPM::interpolateParticlesToGrid(const ProcessorGroup*,
					   const PatchSubset* patches,
					   const MaterialSubset* ,
					   DataWarehouse* old_dw,
					   DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing interpolateParticlesToGrid on patch " << patch->getID()
	       <<"\t\t MPM"<< endl;

    int numMatls = d_sharedState->getNumMPMMatls();

    NCVariable<double> gmassglobal,gtempglobal;
    NCVariable<Vector> gvelglobal;
    new_dw->allocateAndPut(gmassglobal, lb->gMassLabel,
		     d_sharedState->getAllInOneMatl()->get(0), patch);
    new_dw->allocateAndPut(gtempglobal, lb->gTemperatureLabel,
		     d_sharedState->getAllInOneMatl()->get(0), patch);
    new_dw->allocateAndPut(gvelglobal, lb->gVelocityLabel,
		     d_sharedState->getAllInOneMatl()->get(0), patch);
    gmassglobal.initialize(d_SMALL_NUM_MPM);
    gtempglobal.initialize(0.0);
    gvelglobal.initialize(Vector(0.0));

    Ghost::GhostType  gan = Ghost::AroundNodes;
    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      // Create arrays for the particle data
      constParticleVariable<Point>  px;
      constParticleVariable<double> pmass, pvolume, pTemperature, pSp_vol;
      constParticleVariable<Vector> pvelocity, pexternalforce,psize;

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
                                                       gan, NGP, lb->pXLabel);

      old_dw->get(px,             lb->pXLabel,             pset);
      old_dw->get(pmass,          lb->pMassLabel,          pset);
      old_dw->get(pvolume,        lb->pVolumeLabel,        pset);
      old_dw->get(pSp_vol,        lb->pSp_volLabel,        pset);
      old_dw->get(pvelocity,      lb->pVelocityLabel,      pset);
      old_dw->get(pTemperature,   lb->pTemperatureLabel,   pset);
      old_dw->get(pexternalforce, lb->pExternalForceLabel, pset);
      if(d_8or27==27){
        old_dw->get(psize,        lb->pSizeLabel,          pset);
      }

      // Create arrays for the grid data
      NCVariable<double> gmass;
      NCVariable<double> gvolume;
      NCVariable<Vector> gvelocity;
      NCVariable<Vector> gexternalforce;
      NCVariable<double> gexternalheatrate;
      NCVariable<double> gTemperature;
      NCVariable<double> gSp_vol;
      NCVariable<double> gTemperatureNoBC;
      NCVariable<double> gnumnearparticles;

      new_dw->allocateAndPut(gmass,            lb->gMassLabel,       dwi,patch);   
      new_dw->allocateAndPut(gSp_vol,          lb->gSp_volLabel,     dwi,patch);
      new_dw->allocateAndPut(gvolume,          lb->gVolumeLabel,     dwi,patch);
      new_dw->allocateAndPut(gvelocity,        lb->gVelocityLabel,   dwi,patch);
      new_dw->allocateAndPut(gTemperature,     lb->gTemperatureLabel,dwi,patch);
      new_dw->allocateAndPut(gTemperatureNoBC, lb->gTemperatureNoBCLabel,
							             dwi,patch);
      new_dw->allocateAndPut(gexternalforce,   lb->gExternalForceLabel,
                                                                     dwi,patch);
      new_dw->allocateAndPut(gexternalheatrate,lb->gExternalHeatRateLabel,
							             dwi,patch);
      new_dw->allocateAndPut(gnumnearparticles,lb->gNumNearParticlesLabel,
							             dwi,patch);

      gmass.initialize(d_SMALL_NUM_MPM);
      gvolume.initialize(0);
      gvelocity.initialize(Vector(0,0,0));
      gexternalforce.initialize(Vector(0,0,0));
      gTemperature.initialize(0);
      gTemperatureNoBC.initialize(0);
      gexternalheatrate.initialize(0);
      gnumnearparticles.initialize(0.);
      gSp_vol.initialize(0.);

      // Interpolate particle data to Grid data.
      // This currently consists of the particle velocity and mass
      // Need to compute the lumped global mass matrix and velocity
      // Vector from the individual mass matrix and velocity vector
      // GridMass * GridVelocity =  S^T*M_D*ParticleVelocity
      
      double totalmass = 0;
      Vector total_mom(0.0,0.0,0.0);

      IntVector ni[MAX_BASIS];
      double S[MAX_BASIS];
      Vector pmom;

      for(ParticleSubset::iterator iter = pset->begin();
						iter != pset->end(); iter++){
	  particleIndex idx = *iter;

	  // Get the node indices that surround the cell
          if(d_8or27==8){
  	    patch->findCellAndWeights(px[idx], ni, S);
          }
          else if(d_8or27==27){
  	    patch->findCellAndWeights27(px[idx], ni, S, psize[idx]);
          }

          pmom = pvelocity[idx]*pmass[idx];
          total_mom += pvelocity[idx]*pmass[idx];

	  // Add each particles contribution to the local mass & velocity 
	  // Must use the node indices
	  for(int k = 0; k < d_8or27; k++) {
	    if(patch->containsNode(ni[k])) {
	       gmass[ni[k]]          += pmass[idx]                     * S[k];
	       gvelocity[ni[k]]      += pmom                           * S[k];
	       gvolume[ni[k]]        += pvolume[idx]                   * S[k];
	       gexternalforce[ni[k]] += pexternalforce[idx]            * S[k];
	       gTemperature[ni[k]]   += pTemperature[idx] * pmass[idx] * S[k];
//             gexternalheatrate[ni[k]] += pexternalheatrate[idx]      * S[k];
               gnumnearparticles[ni[k]] += 1.0;
              gSp_vol[ni[k]]        += pSp_vol[idx]  * pmass[idx]     *S[k];
	    }
	  }
        }

      for(NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
        IntVector c = *iter; 
        totalmass       += gmass[c];
        gmassglobal[c]  += gmass[c];
        gvelglobal[c]   += gvelocity[c];
        gvelocity[c]    /= gmass[c];
        gtempglobal[c]  += gTemperature[c];
        gTemperature[c] /= gmass[c];
        gTemperatureNoBC[c] = gTemperature[c];
        gSp_vol[c]      /= gmass[c];
      }

      // Apply grid boundary conditions to the velocity before storing the data
      IntVector offset =  IntVector(0,0,0);

      for(Patch::FaceType face = Patch::startFace;
	face <= Patch::endFace; face=Patch::nextFace(face)){
        const BoundCondBase *vel_bcs, *temp_bcs, *sym_bcs;
        if (patch->getBCType(face) == Patch::None) {
	   vel_bcs  = patch->getBCValues(dwi,"Velocity",face);
	   temp_bcs = patch->getBCValues(dwi,"Temperature",face);
	   sym_bcs  = patch->getBCValues(dwi,"Symmetric",face);
        } else
          continue;

	  if (vel_bcs != 0) {
	    const VelocityBoundCond* bc =
	      dynamic_cast<const VelocityBoundCond*>(vel_bcs);
	    if (bc->getKind() == "Dirichlet") {
	      //cout << "Velocity bc value = " << bc->getValue() << endl;
	      fillFace(gvelocity,patch, face,bc->getValue(),offset);
	    }
	  }
	  if (sym_bcs != 0) {
	     fillFaceNormal(gvelocity,patch, face,offset);
	  }
	  if (temp_bcs != 0) {
            const TemperatureBoundCond* bc =
	      dynamic_cast<const TemperatureBoundCond*>(temp_bcs);
            if (bc->getKind() == "Dirichlet") {
              fillFace(gTemperature,patch, face,bc->getValue(),offset);
	    }
	  }
      }

      new_dw->put(sum_vartype(totalmass), lb->TotalMassLabel);

    }  // End loop over materials

    for(NodeIterator iter = patch->getNodeIterator(); !iter.done();iter++){
        IntVector c = *iter;
        gtempglobal[c] /= gmassglobal[c];
        gvelglobal[c] /= gmassglobal[c];
    }
  }  // End loop over patches
}

void SerialMPM::computeStressTensor(const ProcessorGroup*,
				    const PatchSubset* patches,
				    const MaterialSubset* ,
				    DataWarehouse* old_dw,
				    DataWarehouse* new_dw)
{

  cout_doing <<"Doint computeStressTensor " <<"\t\t\t\t MPM"<< endl;

   for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial(m);
      ConstitutiveModel* cm = mpm_matl->getConstitutiveModel();
      cm->computeStressTensor(patches, mpm_matl, old_dw, new_dw);
   }
}

void SerialMPM::computeArtificialViscosity(const ProcessorGroup*,
                                           const PatchSubset* patches,
                                           const MaterialSubset* ,
                                           DataWarehouse* old_dw,
                                           DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing computeArtificialViscosity on patch " << patch->getID()
	       <<"\t\t MPM"<< endl;

    Ghost::GhostType  gac   = Ghost::AroundCells;

    int numMatls = d_sharedState->getNumMPMMatls();
    for(int m = 0; m < numMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

      constNCVariable<Vector> gvelocity;
      ParticleVariable<double> p_q;
      constParticleVariable<Vector> psize;
      constParticleVariable<Point> px;
      constParticleVariable<double> pmass,pvol_def;
      new_dw->get(gvelocity, lb->gVelocityLabel, dwi,patch, gac, NGN);
      old_dw->get(px,        lb->pXLabel,                      pset);
      old_dw->get(pmass,     lb->pMassLabel,                   pset);
      new_dw->get(pvol_def,  lb->pVolumeDeformedLabel,         pset);
      new_dw->allocateAndPut(p_q,    lb->p_qLabel,             pset);
      if(d_8or27==27){
        old_dw->get(psize,   lb->pSizeLabel,                   pset);
      }

      Matrix3 velGrad;
      Vector dx = patch->dCell();
      double oodx[3] = {1./dx.x(), 1./dx.y(), 1./dx.z()};
      double dx_ave = (dx.x() + dx.y() + dx.z())/3.0;

      double K = 1./mpm_matl->getConstitutiveModel()->getCompressibility();
      double c_dil;

      for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++){
         particleIndex idx = *iter;

       // Get the node indices that surround the cell
       IntVector ni[MAX_BASIS];
       Vector d_S[MAX_BASIS];

       if(d_8or27==8){
          patch->findCellAndShapeDerivatives(px[idx], ni, d_S);
       }
       else if(d_8or27==27){
         patch->findCellAndShapeDerivatives27(px[idx], ni, d_S,psize[idx]);
       }

       velGrad.set(0.0);
       for(int k = 0; k < d_8or27; k++) {
         const Vector& gvel = gvelocity[ni[k]];
         for(int j = 0; j<3; j++){
            double d_SXoodx = d_S[k][j] * oodx[j];
            for(int i = 0; i<3; i++) {
                velGrad(i+1,j+1) += gvel[i] * d_SXoodx;
            }
         }
       }

       Matrix3 D = (velGrad + velGrad.Transpose())*.5;

       double DTrace = D.Trace();
       p_q[idx] = 0.0;
       if(DTrace<0.){
         c_dil = sqrt(K*pvol_def[idx]/pmass[idx]);
         p_q[idx] = (.2*fabs(c_dil*DTrace*dx_ave) +
                      2.*(DTrace*DTrace*dx_ave*dx_ave))*
                     (pmass[idx]/pvol_def[idx]);
       }

      }
    }
  }

}

void SerialMPM::computeInternalForce(const ProcessorGroup*,
				     const PatchSubset* patches,
				     const MaterialSubset* ,
				     DataWarehouse* old_dw,
				     DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing computeInternalForce on patch " << patch->getID()
	       <<"\t\t\t MPM"<< endl;

    Vector dx = patch->dCell();
    double oodx[3];
    oodx[0] = 1.0/dx.x();
    oodx[1] = 1.0/dx.y();
    oodx[2] = 1.0/dx.z();
    Matrix3 Id;
    Id.Identity();

    int numMPMMatls = d_sharedState->getNumMPMMatls();

#ifdef INTEGRAL_TRACTION
    double integralTraction = 0.;
    double integralArea = 0.;
#endif

    NCVariable<Matrix3>       gstressglobal;
    constNCVariable<double>   gmassglobal;
    new_dw->get(gmassglobal,  lb->gMassLabel,
		d_sharedState->getAllInOneMatl()->get(0), patch, Ghost::None,0);
    new_dw->allocateAndPut(gstressglobal, lb->gStressForSavingLabel, 
		     d_sharedState->getAllInOneMatl()->get(0), patch);

    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      // Create arrays for the particle position, volume
      // and the constitutive model
      constParticleVariable<Point>   px;
      constParticleVariable<double>  pvol, pmass;
      constParticleVariable<double>  p_pressure;
      constParticleVariable<double>  p_q;
      constParticleVariable<Matrix3> pstress;
      constParticleVariable<Vector>  psize;
      NCVariable<Vector>             internalforce;
      NCVariable<Matrix3>            gstress;
      constNCVariable<double>        gmass;

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
					       Ghost::AroundNodes, NGP,
					       lb->pXLabel);

      old_dw->get(px,      lb->pXLabel,                      pset);
      old_dw->get(pmass,   lb->pMassLabel,                   pset);
      new_dw->get(pvol,    lb->pVolumeDeformedLabel,         pset);
      new_dw->get(pstress, lb->pStressLabel_preReloc,        pset);
      if(d_8or27==27){
        old_dw->get(psize, lb->pSizeLabel,                   pset);
      }
      new_dw->get(gmass,   lb->gMassLabel, dwi, patch, Ghost::None, 0);

      new_dw->allocateAndPut(gstress,      lb->gStressForSavingLabel,dwi,patch);
      new_dw->allocateAndPut(internalforce,lb->gInternalForceLabel,  dwi,patch);

      if(d_with_ice){
        new_dw->get(p_pressure,lb->pPressureLabel, pset);
      }
      else {
	ParticleVariable<double>  p_pressure_create;
	new_dw->allocateTemporary(p_pressure_create,  pset);
	for(ParticleSubset::iterator it = pset->begin();it != pset->end();it++){
	   p_pressure_create[*it]=0.0;
	}
	p_pressure = p_pressure_create; // reference created data
      }

      if(d_artificial_viscosity){
        new_dw->get(p_q,lb->p_qLabel, pset);
      }
      else {
	ParticleVariable<double>  p_q_create;
	new_dw->allocateTemporary(p_q_create,  pset);
	for(ParticleSubset::iterator it = pset->begin();it != pset->end();it++){
	   p_q_create[*it]=0.0;
	}
	p_q = p_q_create; // reference created data
      }


      internalforce.initialize(Vector(0,0,0));
      IntVector ni[MAX_BASIS];
      double S[MAX_BASIS];
      Vector d_S[MAX_BASIS];
      Matrix3 stressmass;
      Matrix3 stresspress;

      for(ParticleSubset::iterator iter = pset->begin();
						iter != pset->end(); iter++){
          particleIndex idx = *iter;
  
          // Get the node indices that surround the cell
          if(d_8or27==8){
            patch->findCellAndWeightsAndShapeDerivatives(px[idx], ni, S,  d_S);
          }
          else if(d_8or27==27){
            patch->findCellAndWeightsAndShapeDerivatives27(px[idx], ni, S,d_S,
                                                           psize[idx]);
          }

          stressmass  = pstress[idx]*pmass[idx];
          stresspress = pstress[idx] + Id*p_pressure[idx] - Id*p_q[idx];

          for (int k = 0; k < d_8or27; k++){
	    if(patch->containsNode(ni[k])){
	       Vector div(d_S[k].x()*oodx[0],d_S[k].y()*oodx[1],
						d_S[k].z()*oodx[2]);
	       internalforce[ni[k]] -= (div * stresspress)  * pvol[idx];
               gstress[ni[k]]       += stressmass * S[k];
	     }
          }
      }

    for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
        IntVector c = *iter;
        gstressglobal[c] += gstress[c];
        gstress[c] /= gmass[c];
    }

#ifdef INTEGRAL_TRACTION
    IntVector low = patch-> getInteriorNodeLowIndex();
    IntVector hi  = patch-> getInteriorNodeHighIndex();
    for(Patch::FaceType face = Patch::startFace;
        face <= Patch::endFace; face=Patch::nextFace(face)){

        // I assume we have the patch variable
        // Check if the face is on the boundary
         Patch::BCType bc_type = patch->getBCType(face);
         if (bc_type == Patch::None) {
           // We are on the boundary, i.e. not on an interior patch
           // boundary, so do the traction accumulation . . .
           if(face==Patch::zminus){
             int K=low.z();
             for (int i = low.x(); i<hi.x(); i++) {
              for (int j = low.y(); j<hi.y(); j++) {
		integralTraction +=
			gstress[IntVector(i,j,K)](3,3)*dx.x()*dx.y();
		if(fabs(gstress[IntVector(i,j,K)](3,3)) > 1.e-12){
		  integralArea+=dx.x()*dx.y();
                }
              }
             }
	   }
        } // end of if (bc_type == Patch::None)
    }
#endif
    //__________________________________
    // Set internal force = 0 on symmetric boundaries
    for(Patch::FaceType face = Patch::startFace;
	  face <= Patch::endFace; face=Patch::nextFace(face)){
      const BoundCondBase *sym_bcs;
      if (patch->getBCType(face) == Patch::None) {
        sym_bcs  = patch->getBCValues(dwi,"Symmetric",face);
      } else
        continue;
      if (sym_bcs != 0) {
        IntVector offset(0,0,0);
        fillFaceNormal(internalforce,patch, face,offset);
      }
    }
#ifdef KUMAR
    internalforce.initialize(Vector(0,0,0));
#endif
  }
#ifdef INTEGRAL_TRACTION
  if(integralArea > 0.){
    integralTraction=integralTraction/integralArea;
  }
  else{
    integralTraction=0.;
  }
  new_dw->put(sum_vartype(integralTraction), lb->NTractionZMinusLabel);
#endif

    for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++) {
      IntVector c = *iter;
      gstressglobal[c] /= gmassglobal[c];
    }
  }
}

void SerialMPM::computeInternalHeatRate(const ProcessorGroup*,
				        const PatchSubset* patches,
					const MaterialSubset* ,
				        DataWarehouse* old_dw,
				        DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing computeInternalHeatRate on patch " << patch->getID()
	       <<"\t\t MPM"<< endl;

    Vector dx = patch->dCell();
    double oodx[3];
    oodx[0] = 1.0/dx.x();
    oodx[1] = 1.0/dx.y();
    oodx[2] = 1.0/dx.z();

    Ghost::GhostType  gac = Ghost::AroundCells;
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      double thermalConductivity = mpm_matl->getThermalConductivity();
      
      constParticleVariable<Point>  px;
      constParticleVariable<double> pvol;
      constParticleVariable<Vector> psize;
      ParticleVariable<Vector>      pTemperatureGradient;
      constNCVariable<double>       gTemperature;
      NCVariable<double>            internalHeatRate;

      ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch,
					       Ghost::AroundNodes, NGP,
					       lb->pXLabel);

      old_dw->get(px,           lb->pXLabel,              pset);
      new_dw->get(pvol,         lb->pVolumeDeformedLabel, pset);
      if(d_8or27==27){
        old_dw->get(psize,      lb->pSizeLabel,           pset);
      }

      new_dw->get(gTemperature, lb->gTemperatureLabel,   dwi, patch, gac,2*NGN);
      new_dw->allocateAndPut(internalHeatRate, lb->gInternalHeatRateLabel,
                                                                    dwi, patch);
      new_dw->allocateTemporary(pTemperatureGradient, pset);
  
      internalHeatRate.initialize(0.);

      // First compute the temperature gradient at each particle
      IntVector ni[MAX_BASIS];
      Vector d_S[MAX_BASIS];

      for(ParticleSubset::iterator iter = pset->begin();
         iter != pset->end(); iter++){
         particleIndex idx = *iter;

         // Get the node indices that surround the cell
         if(d_8or27==8){
            patch->findCellAndShapeDerivatives(px[idx], ni, d_S);
         }
         else if(d_8or27==27){
            patch->findCellAndShapeDerivatives27(px[idx], ni, d_S, psize[idx]);
         }

	 pTemperatureGradient[idx] = Vector(0.0,0.0,0.0);
         for (int k = 0; k < d_8or27; k++){
             for (int j = 0; j<3; j++) {
               pTemperatureGradient[idx][j] += 
                    gTemperature[ni[k]] * d_S[k][j] * oodx[j];
             }
         }
      }


      for(ParticleSubset::iterator iter = pset->begin();
         iter != pset->end(); iter++){
         particleIndex idx = *iter;
  
         // Get the node indices that surround the cell
         if(d_8or27==8){
            patch->findCellAndShapeDerivatives(px[idx], ni, d_S);
         }
         else if(d_8or27==27){
            patch->findCellAndShapeDerivatives27(px[idx], ni, d_S, psize[idx]);
         }

         for (int k = 0; k < d_8or27; k++){
	   if(patch->containsNode(ni[k])){
             Vector div(d_S[k].x()*oodx[0],d_S[k].y()*oodx[1],
						d_S[k].z()*oodx[2]);
	     internalHeatRate[ni[k]] -= Dot( div, pTemperatureGradient[idx]) * 
	                                pvol[idx] * thermalConductivity;
	   }
         }
      }
    }  // End of loop over materials
  }  // End of loop over patches
}


void SerialMPM::solveEquationsMotion(const ProcessorGroup*,
				     const PatchSubset* patches,
				     const MaterialSubset*,
				     DataWarehouse* old_dw,
				     DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing solveEquationsMotion on patch " << patch->getID()
	       <<"\t\t\t MPM"<< endl;

    Vector gravity = d_sharedState->getGravity();
    delt_vartype delT;
    old_dw->get(delT, d_sharedState->get_delt_label() );

    Ghost::GhostType  gnone = Ghost::None;
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      // Get required variables for this patch
      constNCVariable<Vector> internalforce;
      constNCVariable<Vector> externalforce;
      constNCVariable<Vector> gradPAccNC;  // for MPMICE
      constNCVariable<Vector> AccArchesNC; // for MPMArches
      constNCVariable<double> mass;
 
      new_dw->get(internalforce, lb->gInternalForceLabel, dwi, patch, gnone, 0);
      new_dw->get(externalforce, lb->gExternalForceLabel, dwi, patch, gnone, 0);
      new_dw->get(mass,          lb->gMassLabel,          dwi, patch, gnone, 0);
      if(d_with_ice){
         new_dw->get(gradPAccNC, lb->gradPAccNCLabel,     dwi, patch, gnone, 0);
      }
      else{
  	 NCVariable<Vector> gradPAccNC_create;
	 new_dw->allocateTemporary(gradPAccNC_create,  patch);
	 gradPAccNC_create.initialize(Vector(0.,0.,0.));
	 gradPAccNC = gradPAccNC_create; // reference created data
      }
      if(d_with_arches){
         new_dw->get(AccArchesNC,lb->AccArchesNCLabel,    dwi, patch, gnone, 0);
      }
      else{
  	 NCVariable<Vector> AccArchesNC_create;
	 new_dw->allocateTemporary(AccArchesNC_create,  patch);
	 AccArchesNC_create.initialize(Vector(0.,0.,0.));
	 AccArchesNC = AccArchesNC_create; // reference created data	 
      }

//    Uncomment to use damping
//    constNCVariable<Vector> velocity;
//    new_dw->get(velocity,      lb->gVelocityLabel,      dwi, patch, gnone, 0);

      // Create variables for the results
      NCVariable<Vector> acceleration;
      new_dw->allocateAndPut(acceleration, lb->gAccelerationLabel, dwi, patch);
      acceleration.initialize(Vector(0.,0.,0.));

       for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
         IntVector c = *iter;
         acceleration[c] =
                 (internalforce[c] + externalforce[c])/mass[c] +
                 gravity + gradPAccNC[c] + AccArchesNC[c];
//         acceleration[c] =
//            (internalforce[c] + externalforce[c]
//                                        -1000.*velocity[c]*mass[c])/mass[c]
//                                + gravity + gradPAccNC[c] + AccArchesNC[c];
       }
    }
  }
}

void SerialMPM::solveHeatEquations(const ProcessorGroup*,
				   const PatchSubset* patches,
				   const MaterialSubset* ,
				   DataWarehouse* /*old_dw*/,
				   DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing solveHeatEquations on patch " << patch->getID()
	       <<"\t\t\t MPM"<< endl;

    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      double specificHeat = mpm_matl->getSpecificHeat();
     
      // Get required variables for this patch
      constNCVariable<double> mass,externalHeatRate,gvolume;
      constNCVariable<double> thermalContactHeatExchangeRate;
      NCVariable<double> internalHeatRate;
      constNCVariable<double> htrate_gasNC; // for MPMArches
            
      new_dw->get(mass,    lb->gMassLabel,      dwi, patch, Ghost::None, 0);
      new_dw->get(gvolume, lb->gVolumeLabel,    dwi, patch, Ghost::None, 0);
      new_dw->getCopy(internalHeatRate, lb->gInternalHeatRateLabel,
                                                dwi, patch, Ghost::None, 0);
      new_dw->get(externalHeatRate,     lb->gExternalHeatRateLabel,
                                                dwi, patch, Ghost::None, 0);

      new_dw->get(thermalContactHeatExchangeRate,
                  lb->gThermalContactHeatExchangeRateLabel,
                                                dwi, patch, Ghost::None, 0);

      if (d_with_arches) {
	new_dw->get(htrate_gasNC,lb->heaTranSolid_NCLabel,    
		    dwi, patch, Ghost::None, 0);
      }
      else{
	NCVariable<double> htrate_gasNC_create;
	new_dw->allocateTemporary(htrate_gasNC_create, patch);				  

	htrate_gasNC_create.initialize(0.0);
	htrate_gasNC = htrate_gasNC_create; // reference created data
      }

      Vector dx = patch->dCell();
      for(Patch::FaceType face = Patch::startFace;
	  face <= Patch::endFace; face=Patch::nextFace(face)){
	const BoundCondBase* temp_bcs;
        if (patch->getBCType(face) == Patch::None) {
	   temp_bcs = patch->getBCValues(dwi,"Temperature",face);
        } else
          continue;

	if (temp_bcs != 0) {
            const TemperatureBoundCond* bc =
	      dynamic_cast<const TemperatureBoundCond*>(temp_bcs);
            if (bc->getKind() == "Neumann"){
	      double value = bc->getValue();

             IntVector low = patch->getInteriorNodeLowIndex();
             IntVector hi  = patch->getInteriorNodeHighIndex();     
              if(face==Patch::xplus || face==Patch::xminus){
                int I = 0;
                if(face==Patch::xminus){ I=low.x(); }
                if(face==Patch::xplus){ I=hi.x()-1; }
                for (int j = low.y(); j<hi.y(); j++) {
                  for (int k = low.z(); k<hi.z(); k++) {
                    internalHeatRate[IntVector(I,j,k)] +=
				value*(2.0*gvolume[IntVector(I,j,k)]/dx.x());
                  }
                }
              }
              if(face==Patch::yplus || face==Patch::yminus){
                int J = 0;
                if(face==Patch::yminus){ J=low.y(); }
                if(face==Patch::yplus){ J=hi.y()-1; }
                for (int i = low.x(); i<hi.x(); i++) {
                  for (int k = low.z(); k<hi.z(); k++) {
                    internalHeatRate[IntVector(i,J,k)] +=
				value*(2.0*gvolume[IntVector(i,J,k)]/dx.y());
                  }
                }
              }
              if(face==Patch::zplus || face==Patch::zminus){
                int K = 0;
                if(face==Patch::zminus){ K=low.z(); }
                if(face==Patch::zplus){ K=hi.z()-1; }
                for (int i = low.x(); i<hi.x(); i++) {
                  for (int j = low.y(); j<hi.y(); j++) {
                    internalHeatRate[IntVector(i,j,K)] +=
				value*(2.0*gvolume[IntVector(i,j,K)]/dx.z());
                  }
                }
              }
            }
          }
        
      }

      // Create variables for the results
      NCVariable<double> tempRate;
      new_dw->allocateAndPut(tempRate, lb->gTemperatureRateLabel, dwi, patch);
      tempRate.initialize(0.0);

      for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
          IntVector c = *iter;
	  tempRate[c] = (internalHeatRate[c]
		      +  externalHeatRate[c]
	              +  htrate_gasNC[c]) /
   		        (mass[c] * specificHeat) + 
                         thermalContactHeatExchangeRate[c];
      }
    }
  }
}


void SerialMPM::integrateAcceleration(const ProcessorGroup*,
				      const PatchSubset* patches,
				      const MaterialSubset*,
				      DataWarehouse* old_dw,
				      DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing integrateAcceleration on patch " << patch->getID()
	       <<"\t\t\t MPM"<< endl;

    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      constNCVariable<Vector>  acceleration, velocity;
      delt_vartype delT;

      new_dw->get(acceleration,lb->gAccelerationLabel,dwi, patch,Ghost::None,0);
      new_dw->get(velocity,    lb->gVelocityLabel,    dwi, patch,Ghost::None,0);

      old_dw->get(delT, d_sharedState->get_delt_label() );

      // Create variables for the results
      NCVariable<Vector> velocity_star;
      new_dw->allocateAndPut(velocity_star, lb->gVelocityStarLabel, dwi, patch);
      velocity_star.initialize(0.0);

      for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
        IntVector c = *iter;
	velocity_star[c] = velocity[c] + acceleration[c] * delT;
      }
    }
  }
}

void SerialMPM::integrateTemperatureRate(const ProcessorGroup*,
					 const PatchSubset* patches,
					 const MaterialSubset*,
					 DataWarehouse* old_dw,
					 DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing integrateTemperatureRate on patch " << patch->getID()
	       << "\t\t MPM"<< endl;

    Ghost::GhostType  gnone = Ghost::None;
    for(int m = 0; m < d_sharedState->getNumMPMMatls(); m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      constNCVariable<double> temperature;
      constNCVariable<double> temperatureRate;
      NCVariable<double> tempStar;
      delt_vartype delT;
 
      new_dw->get(temperature,     lb->gTemperatureLabel,    dwi,patch,gnone,0);
      new_dw->get(temperatureRate, lb->gTemperatureRateLabel,dwi,patch,gnone,0);

      old_dw->get(delT, d_sharedState->get_delt_label() );

      new_dw->allocateAndPut(tempStar, lb->gTemperatureStarLabel, dwi,patch);
      tempStar.initialize(0.0);

      for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){
        IntVector c = *iter;
        tempStar[c] = temperature[c] + temperatureRate[c] * delT;
      }
    }
  }
}

void SerialMPM::setGridBoundaryConditions(const ProcessorGroup*,
						const PatchSubset* patches,
						const MaterialSubset* ,
						DataWarehouse* old_dw,
						DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);

    cout_doing <<"Doing setGridBoundaryConditions on patch " << patch->getID()
	       <<"\t\t MPM"<< endl;

    int numMPMMatls=d_sharedState->getNumMPMMatls();
    
    delt_vartype delT;            
    old_dw->get(delT, d_sharedState->get_delt_label() );
                      
    for(int m = 0; m < numMPMMatls; m++){
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      NCVariable<Vector> gvelocity_star, gacceleration;
      NCVariable<double> gTempRate;
      constNCVariable<double> gTemperatureNoBC;
      
      new_dw->getModifiable(gacceleration, lb->gAccelerationLabel,   dwi,patch);
      new_dw->getModifiable(gvelocity_star,lb->gVelocityStarLabel,   dwi,patch);
      new_dw->getModifiable(gTempRate,     lb->gTemperatureRateLabel,dwi,patch);
      new_dw->get(gTemperatureNoBC, lb->gTemperatureNoBCLabel,
                                                    dwi, patch, Ghost::None, 0);
      // Apply grid boundary conditions to the velocity_star and
      // acceleration before interpolating back to the particles
      IntVector offset(0,0,0);
      for(Patch::FaceType face = Patch::startFace;
	  face <= Patch::endFace; face=Patch::nextFace(face)){
        const BoundCondBase *vel_bcs, *temp_bcs, *sym_bcs;
        if (patch->getBCType(face) == Patch::None) {
	   vel_bcs  = patch->getBCValues(dwi,"Velocity",   face);
	   temp_bcs = patch->getBCValues(dwi,"Temperature",face);
	   sym_bcs  = patch->getBCValues(dwi,"Symmetric",  face);
        } else
          continue;
         //__________________________________
         // Velocity and Acceleration
	  if (vel_bcs != 0) {
	    const VelocityBoundCond* bc = 
	      dynamic_cast<const VelocityBoundCond*>(vel_bcs);
	    //cout << "Velocity bc value = " << bc->getValue() << endl;
	    if (bc->getKind() == "Dirichlet") {
	      fillFace(gvelocity_star,patch, face,bc->getValue(),     offset);
	      fillFace(gacceleration, patch, face,Vector(0.0,0.0,0.0),offset);
	    }
	  }
	  if (sym_bcs != 0) {
	     fillFaceNormal(gvelocity_star,patch, face,offset);
	     fillFaceNormal(gacceleration, patch, face,offset);
	  }
         //__________________________________
         // Temperature BC
	  if (temp_bcs != 0) {
	    const TemperatureBoundCond* bc = 
	      dynamic_cast<const TemperatureBoundCond*>(temp_bcs);
	    if (bc->getKind() == "Dirichlet") {
	      //cout << "Temperature bc value = " << bc->getValue() << endl;
              
            IntVector low = patch->getInteriorNodeLowIndex();
            IntVector hi  = patch->getInteriorNodeHighIndex();
	     double boundTemp = bc->getValue();
	     if(face==Patch::xplus || face==Patch::xminus){
		int I = 0;
		if(face==Patch::xminus){ I=low.x(); }
		if(face==Patch::xplus){  I=hi.x()-1; }
		for (int j = low.y(); j<hi.y(); j++) { 
		  for (int k = low.z(); k<hi.z(); k++) {
		    gTempRate[IntVector(I,j,k)] +=
		      (boundTemp - gTemperatureNoBC[IntVector(I,j,k)])/delT;
		  }
		}
	     }
	     if(face==Patch::yplus || face==Patch::yminus){
	       int J = 0;
	       if(face==Patch::yminus){ J=low.y(); }
	       if(face==Patch::yplus){  J=hi.y()-1; }
	       for (int i = low.x(); i<hi.x(); i++) {
		  for (int k = low.z(); k<hi.z(); k++) {
		    gTempRate[IntVector(i,J,k)] +=
		      (boundTemp - gTemperatureNoBC[IntVector(i,J,k)])/delT;
		  }
	       }
	     }
	     if(face==Patch::zplus || face==Patch::zminus){
	       int K = 0;
	       if(face==Patch::zminus){ K=low.z(); }
	       if(face==Patch::zplus){  K=hi.z()-1; }
	       for (int i = low.x(); i<hi.x(); i++) {
		  for (int j = low.y(); j<hi.y(); j++) {
		    gTempRate[IntVector(i,j,K)] +=
		      (boundTemp - gTemperatureNoBC[IntVector(i,j,K)])/delT;
		  }
	       }
	     }
	   }  // if(dirichlet)
	   if (bc->getKind() == "Neumann") {
	      //cout << "bc value = " << bc->getValue() << endl;
	   }
	 }  //if(temp_bc}
      }  // patch face loop

    } // matl loop
  }  // patch loop
}

void SerialMPM::applyExternalLoads(const ProcessorGroup*,
                                   const PatchSubset* patches,
                                   const MaterialSubset* ,
                                   DataWarehouse* old_dw,
                                   DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
   const Patch* patch = patches->get(p);

   cout_doing <<"Doing applyExternalLoads on patch " 
       << patch->getID() << "\t MPM"<< endl;

   // Place for user defined loading scenarios to be defined,
   // otherwise pExternalForce is just carried forward.

   int numMPMMatls=d_sharedState->getNumMPMMatls();

   for(int m = 0; m < numMPMMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     int dwi = mpm_matl->getDWIndex();

     constParticleVariable<Vector> pexternalForce;
     ParticleVariable<Vector>      pextForceNew;
     ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

     old_dw->get(pexternalForce,          lb->pExternalForceLabel,      pset);
     new_dw->allocateAndPut(pextForceNew, lb->pExtForceLabel_preReloc,  pset);
     pextForceNew.copyData(pexternalForce);

   } // matl loop
  }  // patch loop
}

void SerialMPM::calculateDampingRate(const ProcessorGroup*,
				     const PatchSubset* patches,
				     const MaterialSubset* ,
				     DataWarehouse* old_dw,
				     DataWarehouse* new_dw)
{
  if (d_artificialDampCoeff > 0.0) {
    for(int p=0;p<patches->size();p++){
      const Patch* patch = patches->get(p);

      cout_doing <<"Doing calculateDampingRate on patch " 
		 << patch->getID() << "\t MPM"<< endl;

      double alphaDot = 0.0;
      int numMPMMatls=d_sharedState->getNumMPMMatls();
      for(int m = 0; m < numMPMMatls; m++){
	MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
	int dwi = mpm_matl->getDWIndex();

	// Get the arrays of particle values to be changed
	constParticleVariable<Point> px;
	constParticleVariable<Vector> psize;

	// Get the arrays of grid data on which the new part. values depend
	constNCVariable<Vector> gvelocity_star;

	ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);
	old_dw->get(px, lb->pXLabel, pset);
	if(d_8or27==27) old_dw->get(psize, lb->pSizeLabel, pset);
	Ghost::GhostType  gac = Ghost::AroundCells;
	new_dw->get(gvelocity_star,   lb->gVelocityStarLabel,   dwi,patch,gac,NGP);

	IntVector ni[MAX_BASIS];
	double S[MAX_BASIS];
	Vector d_S[MAX_BASIS];

	// Calculate artificial dampening rate based on the interpolated particle
	// velocities (ref. Ayton et al., 2002, Biophysical Journal, 1026-1038)
	// d(alpha)/dt = 1/Q Sum(vp*^2)
	ParticleSubset::iterator iter = pset->begin();
	for(;iter != pset->end(); iter++){
	  particleIndex idx = *iter;
	  if (d_8or27 == 27) 
	    patch->findCellAndWeightsAndShapeDerivatives27(px[idx],ni,S,d_S,psize[idx]);
	  else
	    patch->findCellAndWeightsAndShapeDerivatives(px[idx],ni,S,d_S);
	  Vector vel(0.0,0.0,0.0);
	  for (int k = 0; k < d_8or27; k++) vel += gvelocity_star[ni[k]]*S[k];
	  alphaDot += Dot(vel,vel);
	}
	alphaDot /= d_artificialDampCoeff;
      } 
      new_dw->put(sum_vartype(alphaDot), d_dampingRateLabel);
    }
  }
}


void SerialMPM::interpolateToParticlesAndUpdate(const ProcessorGroup*,
						const PatchSubset* patches,
						const MaterialSubset* ,
						DataWarehouse* old_dw,
						DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
   const Patch* patch = patches->get(p);

   cout_doing <<"Doing interpolateToParticlesAndUpdate on patch " 
       << patch->getID() << "\t MPM"<< endl;

   // Performs the interpolation from the cell vertices of the grid
   // acceleration and velocity to the particles to update their
   // velocity and position respectively
 
   // DON'T MOVE THESE!!!
   double thermal_energy = 0.0;
   Vector CMX(0.0,0.0,0.0);
   Vector CMV(0.0,0.0,0.0);
   double ke=0;
   int numMPMMatls=d_sharedState->getNumMPMMatls();
   delt_vartype delT;
   old_dw->get(delT, d_sharedState->get_delt_label() );
   bool combustion_problem=false;

   // Artificial Damping 
   double alphaDot = 0.0;
   double alpha = 0.0;
   if (d_artificialDampCoeff > 0.0) {
     max_vartype dampingCoeff; 
     sum_vartype dampingRate;
     old_dw->get(dampingCoeff, d_dampingCoeffLabel);
     new_dw->get(dampingRate, d_dampingRateLabel);
     alpha = (double) dampingCoeff;
     alphaDot = (double) dampingRate;
     alpha += alphaDot*delT; // Calculate damping coefficient from damping rate
     new_dw->put(max_vartype(alpha), d_dampingCoeffLabel);
   }

   for(int m = 0; m < numMPMMatls; m++){
     MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
     int dwi = mpm_matl->getDWIndex();
     // Get the arrays of particle values to be changed
     constParticleVariable<Point> px;
     ParticleVariable<Point> pxnew;
     constParticleVariable<Vector> pvelocity, psize;
     ParticleVariable<Vector> pvelocitynew, psizeNew;
     constParticleVariable<double> pmass, pvolume, pTemperature, pSp_vol;
     ParticleVariable<double> pmassNew,pvolumeNew,pTempNew, pSp_volNew;
     constParticleVariable<long64> pids;
     ParticleVariable<long64> pids_new;

     // Get the arrays of grid data on which the new part. values depend
     constNCVariable<Vector> gvelocity_star, gacceleration;
     constNCVariable<double> gTemperatureRate, gTemperature, gTemperatureNoBC;
     constNCVariable<double> dTdt, massBurnFraction, frictionTempRate;
     constNCVariable<double> gSp_vol_src;

     ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

     old_dw->get(px,                    lb->pXLabel,                     pset);
     old_dw->get(pmass,                 lb->pMassLabel,                  pset);
     old_dw->get(pids,                  lb->pParticleIDLabel,            pset);
     old_dw->get(pSp_vol,               lb->pSp_volLabel,                pset);
     new_dw->get(pvolume,               lb->pVolumeDeformedLabel,        pset);
     old_dw->get(pvelocity,             lb->pVelocityLabel,              pset);
     old_dw->get(pTemperature,          lb->pTemperatureLabel,           pset);

     new_dw->allocateAndPut(pvelocitynew, lb->pVelocityLabel_preReloc,   pset);
     new_dw->allocateAndPut(pxnew,        lb->pXLabel_preReloc,          pset);
     new_dw->allocateAndPut(pmassNew,     lb->pMassLabel_preReloc,       pset);
     new_dw->allocateAndPut(pvolumeNew,   lb->pVolumeLabel_preReloc,     pset);
     new_dw->allocateAndPut(pids_new,     lb->pParticleIDLabel_preReloc, pset);
     new_dw->allocateAndPut(pTempNew,     lb->pTemperatureLabel_preReloc,pset);
     new_dw->allocateAndPut(pSp_volNew,   lb->pSp_volLabel_preReloc,     pset);
     
     ParticleSubset* delset = scinew ParticleSubset
	(pset->getParticleSet(),false,dwi,patch);

     pids_new.copyData(pids);
     if(d_8or27==27){
       old_dw->get(psize,               lb->pSizeLabel,                 pset);
       new_dw->allocateAndPut(psizeNew, lb->pSizeLabel_preReloc,        pset);
       psizeNew.copyData(psize);
     }

     Ghost::GhostType  gac = Ghost::AroundCells;
     new_dw->get(gvelocity_star,   lb->gVelocityStarLabel,   dwi,patch,gac,NGP);
     new_dw->get(gacceleration,    lb->gAccelerationLabel,   dwi,patch,gac,NGP);
     new_dw->get(gTemperatureRate, lb->gTemperatureRateLabel,dwi,patch,gac,NGP);
     new_dw->get(gTemperature,     lb->gTemperatureLabel,    dwi,patch,gac,NGP);
     new_dw->get(gTemperatureNoBC, lb->gTemperatureNoBCLabel,dwi,patch,gac,NGP);
     new_dw->get(frictionTempRate, lb->frictionalWorkLabel,  dwi,patch,gac,NGP);    
     if(d_with_ice){
      new_dw->get(dTdt,            lb->dTdt_NCLabel,         dwi,patch,gac,NGP);
      new_dw->get(gSp_vol_src,     lb->gSp_vol_srcLabel,     dwi,patch,gac,NGP);
      new_dw->get(massBurnFraction,lb->massBurnFractionLabel,dwi,patch,gac,NGP);
     }
     else{
      NCVariable<double> dTdt_create,massBurnFraction_create,gSp_vol_src_create;
      new_dw->allocateTemporary(dTdt_create,                     patch,gac,NGP);
      new_dw->allocateTemporary(gSp_vol_src_create,              patch,gac,NGP);
      new_dw->allocateTemporary(massBurnFraction_create,         patch,gac,NGP);
      dTdt_create.initialize(0.);
      gSp_vol_src_create.initialize(0.);
      massBurnFraction_create.initialize(0.);
      dTdt = dTdt_create;                         // reference created data
      gSp_vol_src = gSp_vol_src_create;           // reference created data
      massBurnFraction = massBurnFraction_create; // reference created data
     }

     double Cp=mpm_matl->getSpecificHeat();
     double rho_init=mpm_matl->getInitialDensity();

     IntVector ni[MAX_BASIS];
     double S[MAX_BASIS];
     Vector d_S[MAX_BASIS];

     double rho_frac_min = 1.;
     if(mpm_matl->getRxProduct() == Material::reactant){
      combustion_problem=true;
      rho_frac_min = .1;
     }

     for(ParticleSubset::iterator iter = pset->begin();
                                                  iter != pset->end(); iter++){
	  particleIndex idx = *iter;

          // Get the node indices that surround the cell
          if(d_8or27==8){
            patch->findCellAndWeightsAndShapeDerivatives(px[idx], ni, S, d_S);
          }
          else if(d_8or27==27){
            patch->findCellAndWeightsAndShapeDerivatives27(px[idx], ni, S, d_S,
                                                           psize[idx]);
          }

          Vector vel(0.0,0.0,0.0);
          Vector acc(0.0,0.0,0.0);
          double tempRate = 0;
          double burnFraction = 0;
          double sp_vol_dt = 0.0;

          // Accumulate the contribution from each surrounding vertex
          for (int k = 0; k < d_8or27; k++) {
	      vel      += gvelocity_star[ni[k]]  * S[k];
   	      acc      += gacceleration[ni[k]]   * S[k];
             tempRate += (gTemperatureRate[ni[k]] + dTdt[ni[k]] +
			   frictionTempRate[ni[k]])   * S[k];
             burnFraction += massBurnFraction[ni[k]] * S[k];
             sp_vol_dt += gSp_vol_src[ni[k]]   * S[k];
          }

          // Update the particle's position and velocity
          pxnew[idx]           = px[idx] + vel * delT;
          pvelocitynew[idx]    = pvelocity[idx] + (acc - alpha*vel)*delT;
          pTempNew[idx]        = pTemperature[idx] + tempRate * delT;
          pSp_volNew[idx]      = pSp_vol[idx] + sp_vol_dt * delT;

          double rho;
          if(pvolume[idx] > 0.){
             rho = max(pmass[idx]/pvolume[idx],rho_frac_min*rho_init);
          }
	  else{
	    rho = rho_init;
	  }
          pmassNew[idx]        = Max(pmass[idx]*(1.    - burnFraction),0.);
          pvolumeNew[idx]      = pmassNew[idx]/rho;
          if(pmassNew[idx] <= d_min_part_mass ||
            (rho_frac_min < 1.0 && pvelocitynew[idx].length() > d_max_vel)){
	    delset->addParticle(idx);
	  }
	    
          thermal_energy += pTemperature[idx] * pmass[idx] * Cp;
          ke += .5*pmass[idx]*pvelocitynew[idx].length2();
	  CMX = CMX + (pxnew[idx]*pmass[idx]).asVector();
	  CMV += pvelocitynew[idx]*pmass[idx];
     }

     new_dw->deleteParticles(delset);      
   }

   if(combustion_problem){
     if(delT < 5.e-9){
      if(delT < 1.e-10){
        d_min_part_mass = min(d_min_part_mass*2.0,5.e-9);
        cout << "New d_min_part_mass = " << d_min_part_mass << endl;
      }
      else{
        d_min_part_mass = min(d_min_part_mass*2.0,5.e-12);
        cout << "New d_min_part_mass = " << d_min_part_mass << endl;
      }
     }
     else if(delT > 2.e-8){
      d_min_part_mass = max(d_min_part_mass/2.0,3.e-15);
     }
   }

   // DON'T MOVE THESE!!!
   new_dw->put(sum_vartype(ke),     lb->KineticEnergyLabel);
   new_dw->put(sumvec_vartype(CMX), lb->CenterOfMassPositionLabel);
   new_dw->put(sumvec_vartype(CMV), lb->CenterOfMassVelocityLabel);

// cout << "Solid mass lost this timestep = " << massLost << endl;
// cout << "Solid momentum after advection = " << CMV << endl;

// cout << "THERMAL ENERGY " << thermal_energy << endl;
 }
  
}

void SerialMPM::setSharedState(SimulationStateP& ssp)
{
  d_sharedState = ssp;
}
