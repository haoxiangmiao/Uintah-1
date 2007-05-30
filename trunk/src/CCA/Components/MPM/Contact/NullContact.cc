
// NullContact.cc
// One of the derived Contact classes.  This particular
// class is used when no contact is desired.  This would
// be used for example when a single velocity field is
// present in the problem, so doing contact wouldn't make
// sense.
#include <CCA/Components/MPM/Contact/NullContact.h>
#include <SCIRun/Core/Geometry/Vector.h>
#include <SCIRun/Core/Geometry/IntVector.h>
#include <Core/Grid/Grid.h>
#include <Core/Grid/Variables/NCVariable.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/Variables/NodeIterator.h>
#include <Core/Grid/SimulationState.h>
#include <Core/Grid/SimulationStateP.h>
#include <Core/Grid/Task.h>
#include <Core/Labels/MPMLabel.h>
#include <CCA/Ports/DataWarehouse.h>
#include <CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <CCA/Components/MPM/MPMFlags.h>
using namespace Uintah;

NullContact::NullContact(const ProcessorGroup* myworld,
                         SimulationStateP& d_sS,
			 MPMLabel* Mlb,MPMFlags* MFlags)
  : Contact(myworld, Mlb, MFlags, 0)
{
  // Constructor
  d_sharedState = d_sS;
  lb = Mlb;
  flag = MFlags;

}

NullContact::~NullContact()
{
}

void NullContact::outputProblemSpec(ProblemSpecP& ps)
{
  ProblemSpecP contact_ps = ps->appendChild("contact");
  contact_ps->appendElement("type","null");
  d_matls.outputProblemSpec(contact_ps);
}


void NullContact::exMomInterpolated(const ProcessorGroup*,
				    const PatchSubset* patches,
				    const MaterialSubset* matls,
				    DataWarehouse* /*old_dw*/,
				    DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    for(int m=0;m<matls->size();m++){
      NCVariable<Vector> gvelocity;
      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();
      new_dw->getModifiable(   gvelocity, lb->gVelocityLabel, dwi, patch);
    }
  }
}

void NullContact::exMomIntegrated(const ProcessorGroup*,
				    const PatchSubset* patches,
				    const MaterialSubset* matls,
				    DataWarehouse* /*old_dw*/,
				    DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    for(int m=0;m<matls->size();m++){
      NCVariable<Vector> gv_star;
      NCVariable<Vector> gacc;
      NCVariable<double> frictionalWork;

      MPMMaterial* mpm_matl = d_sharedState->getMPMMaterial( m );
      int dwi = mpm_matl->getDWIndex();

      new_dw->getModifiable(gv_star, lb->gVelocityStarLabel,        dwi, patch);
      new_dw->getModifiable(gacc,    lb->gAccelerationLabel,        dwi, patch);
      new_dw->getModifiable(frictionalWork,lb->frictionalWorkLabel, dwi,
                            patch);
    }
  }
}

void NullContact::addComputesAndRequiresInterpolated(SchedulerP & sched,
						const PatchSet* patches,
						const MaterialSet* ms)
{
  Task * t = new Task("NullContact::exMomInterpolated", this, &NullContact::exMomInterpolated);
  
  const MaterialSubset* mss = ms->getUnion();
  t->modifies(lb->gVelocityLabel, mss);
  
  sched->addTask(t, patches, ms);
}

void NullContact::addComputesAndRequiresIntegrated(SchedulerP & sched,
					     const PatchSet* patches,
					     const MaterialSet* ms) 
{
  Task * t = new Task("NullContact::exMomIntegrated", this, &NullContact::exMomIntegrated);
  
  const MaterialSubset* mss = ms->getUnion();
  t->modifies(lb->gVelocityStarLabel, mss);
  t->modifies(lb->gAccelerationLabel, mss);
  t->modifies(lb->frictionalWorkLabel, mss);
  
  sched->addTask(t, patches, ms);
}
