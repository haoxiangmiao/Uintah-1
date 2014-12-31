/*
 * The MIT License
 *
 * Copyright (c) 1997-2014 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <CCA/Components/MPM/ReactionDiffusion/ScalarDiffusionModel.h>
#include <CCA/Components/MPM/ReactionDiffusion/ReactionDiffusionLabel.h>
#include <CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <CCA/Components/MPM/MPMFlags.h>
#include <Core/Labels/MPMLabel.h>
#include <Core/Grid/Task.h>

#include <iostream>
using namespace std;
using namespace Uintah;


ScalarDiffusionModel::ScalarDiffusionModel(ProblemSpecP& ps, MPMFlags* Mflag){
  d_Mflag = Mflag;

  d_lb = scinew MPMLabel;
  d_rdlb = scinew ReactionDiffusionLabel();

  if(d_Mflag->d_8or27==8){
    NGP=1;
    NGN=1;
  } else {
    NGP=2;
    NGN=2;
  }

  if(d_Mflag->d_scalarDiffusion_type == "explicit"){
    do_explicit = true;
  }else{
    do_explicit = false;
  }
}

ScalarDiffusionModel::~ScalarDiffusionModel() {
  delete d_lb;
  delete d_rdlb;
}

void ScalarDiffusionModel::addInitialComputesAndRequires(Task* task, const MPMMaterial* matl,
                                              const PatchSet* patch) const{
  const MaterialSubset* matlset = matl->thisMaterial();
  task->computes(d_rdlb->pConcentrationLabel,      matlset);
}

void ScalarDiffusionModel::initializeSDMData(const Patch* patch, const MPMMaterial* matl,
                                  DataWarehouse* new_dw)
{
  ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWIndex(), patch);

  ParticleVariable<double>  pConcentration;

  new_dw->allocateAndPut(pConcentration,   d_rdlb->pConcentrationLabel, pset);

  for(ParticleSubset::iterator iter = pset->begin();iter != pset->end();iter++){
    pConcentration[*iter] = 0.0;
  }
}

void ScalarDiffusionModel::addInterpolateParticlesToGridCompAndReq(Task* task, const MPMMaterial* matl,
                                                         const PatchSet* patch) const{

  const MaterialSubset* matlset = matl->thisMaterial();
  Ghost::GhostType  gnone = Ghost::None;
  int NGP = 0;

  task->requires(Task::OldDW, d_rdlb->pConcentrationLabel, matlset, gnone);
  task->computes(d_rdlb->pConcentrationLabel,      matlset);
}

void ScalarDiffusionModel::interpolateParticlesToGrid(const Patch* patch, const MPMMaterial* matl,
                                                      DataWarehouse* old_dw, DataWarehouse* new_dw){

  Ghost::GhostType  gnone = Ghost::None;
  constParticleVariable<double> pConcentration;
  int dwi = matl->getDWIndex();
  ParticleSubset* pset = old_dw->getParticleSubset(dwi, patch);

  old_dw->get(pConcentration,    d_rdlb->pConcentrationLabel,  pset);

  ParticleVariable<double> gConcentration;
  //new_dw->allocateAndPut(gConcentration,  d_rdlb->pConcentrationLabel,  dwi,  patch);
  new_dw->allocateAndPut(gConcentration,  d_rdlb->pConcentrationLabel,  pset);

  //gConcentration.initialize(0);
  
  for (ParticleSubset::iterator iter = pset->begin(); iter != pset->end(); iter++){
    particleIndex idx = *iter;
    gConcentration[idx] = pConcentration[idx] + 1;
  }

}
