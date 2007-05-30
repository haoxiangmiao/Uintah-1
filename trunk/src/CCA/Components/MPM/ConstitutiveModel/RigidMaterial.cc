#include "RigidMaterial.h"
#include <CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <SCIRun/Core/Malloc/Allocator.h>
#include <Core/Grid/Patch.h>
#include <CCA/Ports/DataWarehouse.h>
#include <Core/Grid/Variables/NCVariable.h>
#include <Core/Grid/Variables/ParticleSet.h>
#include <Core/Grid/Variables/ParticleVariable.h>
#include <Core/Grid/Task.h>
#include <Core/Grid/Level.h>
#include <Core/Grid/Variables/VarLabel.h>
#include <SCIRun/Core/Math/MinMax.h>
#include <Core/Labels/MPMLabel.h>
#include <Core/Math/Matrix3.h>
#include <Core/Math/Short27.h>
#include <Core/Grid/Variables/NodeIterator.h> 
#include <Core/Grid/Variables/VarTypes.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Exceptions/ParameterNotFound.h>
#include <SCIRun/Core/Malloc/Allocator.h>
#include <sgi_stl_warnings_off.h>
#include <fstream>
#include <iostream>
#include <sgi_stl_warnings_on.h>

using std::cerr;
using namespace Uintah;
using namespace SCIRun;

RigidMaterial::RigidMaterial(ProblemSpecP& ps, MPMFlags* Mflag) : 
  ConstitutiveModel(Mflag), ImplicitCM()
{
  d_initialData.G = 1.0e200;
  ps->get("shear_modulus",d_initialData.G);
  d_initialData.K = 1.0e200;
  ps->get("bulk_modulus",d_initialData.K);
}

RigidMaterial::RigidMaterial(const RigidMaterial* cm) : ConstitutiveModel(cm)
{
  d_initialData.G = cm->d_initialData.G;
  d_initialData.K = cm->d_initialData.K;
}

RigidMaterial::~RigidMaterial()
{
}

void RigidMaterial::outputProblemSpec(ProblemSpecP& ps,bool output_cm_tag)
{
  ProblemSpecP cm_ps = ps;
  if (output_cm_tag) {
    cm_ps = ps->appendChild("constitutive_model");
    cm_ps->setAttribute("type","rigid");
  }

  cm_ps->appendElement("shear_modulus",d_initialData.G);
  cm_ps->appendElement("bulk_modulus",d_initialData.K);
}



RigidMaterial* RigidMaterial::clone()
{
  return scinew RigidMaterial(*this);
}


void 
RigidMaterial::initializeCMData(const Patch* patch,
                                const MPMMaterial* matl,
                                DataWarehouse* new_dw)
{
  // Initialize the variables shared by all constitutive models
  // This method is defined in the ConstitutiveModel base class.
  if (flag->d_integrator == MPMFlags::Implicit) 
    initSharedDataForImplicit(patch, matl, new_dw);
  else {
    initSharedDataForExplicit(patch, matl, new_dw);
    new_dw->put(delt_vartype(1.0e10), lb->delTLabel, patch->getLevel());
  }
}

void 
RigidMaterial::addComputesAndRequires(Task* task,
                                      const MPMMaterial* matl,
                                      const PatchSet* patches) const
{
  // Add the computes and requires that are common to all explicit 
  // constitutive models.  The method is defined in the ConstitutiveModel
  // base class.
  const MaterialSubset* matlset = matl->thisMaterial();
  if (flag->d_integrator == MPMFlags::Implicit) {
    addSharedCRForImplicit(task, matlset, patches);
  } else {
    addSharedCRForExplicit(task, matlset, patches);
  }
}

void 
RigidMaterial::computeStressTensor(const PatchSubset* patches,
                                   const MPMMaterial* matl,
                                   DataWarehouse* old_dw,
                                   DataWarehouse* new_dw)
{
  if (flag->d_integrator == MPMFlags::Implicit) {
    computeStressTensorImplicit(patches, matl, old_dw, new_dw);
    return;
  }
  carryForward(patches, matl, old_dw, new_dw);
}

void 
RigidMaterial::computeStressTensorImplicit(const PatchSubset* patches,
                                           const MPMMaterial* matl,
                                           DataWarehouse* old_dw,
                                           DataWarehouse* new_dw)
{
  int dwi = matl->getDWIndex();
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

    // Carry forward the data common to all constitutive models 
    // when using RigidMPM.
    // This method is defined in the ConstitutiveModel base class.
    carryForwardSharedData(pset, old_dw, new_dw, matl);
    new_dw->put(sum_vartype(0.),     lb->StrainEnergyLabel);
  }
}

void 
RigidMaterial::carryForward(const PatchSubset* patches,
                            const MPMMaterial* matl,
                            DataWarehouse* old_dw,
                            DataWarehouse* new_dw)
{
  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    int dwi = matl->getDWIndex();
    ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

    // Carry forward the data common to all constitutive models 
    // when using RigidMPM.
    // This method is defined in the ConstitutiveModel base class.
    carryForwardSharedData(pset, old_dw, new_dw, matl);
    new_dw->put(delt_vartype(1.0), lb->delTLabel, patch->getLevel());
    new_dw->put(sum_vartype(0.),     lb->StrainEnergyLabel);
  }
}

void
RigidMaterial::addComputesAndRequires(Task* task,
                                      const MPMMaterial* matl,
                                      const PatchSet* patches,
                                      const bool recurse) const
{
  const MaterialSubset* matlset = matl->thisMaterial();
  addSharedCRForImplicit(task, matlset, patches, recurse);
}

void 
RigidMaterial::computeStressTensor(const PatchSubset* patches,
                                   const MPMMaterial* matl,
                                   DataWarehouse* ,
                                   DataWarehouse* new_dw,
                                   Solver* ,
                                   const bool )

{
  int dwi = matl->getDWIndex();
  DataWarehouse* parent_dw = new_dw->getOtherDataWarehouse(Task::ParentOldDW);
  for (int pp = 0; pp < patches->size(); pp++) {
    const Patch* patch = patches->get(pp);
    ParticleSubset* pset = parent_dw->getParticleSubset(dwi, patch);
    carryForwardSharedData(pset, parent_dw, new_dw, matl);
  }
}

void 
RigidMaterial::addParticleState(std::vector<const VarLabel*>& ,
                                std::vector<const VarLabel*>& )
{
}

double 
RigidMaterial::computeRhoMicroCM(double ,
                                 const double ,
                                 const MPMMaterial* matl)
{
  return matl->getInitialDensity();
}

void 
RigidMaterial::computePressEOSCM(double , double& pressure,
                                 double p_ref,
                                 double& dp_drho, double& tmp,
                                 const MPMMaterial* matl)
{
  double K = d_initialData.K;
  double rho_0 = matl->getInitialDensity();
  pressure = p_ref;
  dp_drho  = K/rho_0;
  tmp = dp_drho;  // speed of sound squared
}

double RigidMaterial::getCompressibility()
{
  return (1.0/d_initialData.K);
}

