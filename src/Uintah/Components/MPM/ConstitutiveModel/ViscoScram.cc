
#include "ConstitutiveModelFactory.h"
#include "ViscoScram.h"
#include <SCICore/Malloc/Allocator.h>
#include <Uintah/Grid/Patch.h>
#include <Uintah/Interface/DataWarehouse.h>
#include <Uintah/Grid/NCVariable.h>
#include <Uintah/Grid/ParticleSet.h>
#include <Uintah/Grid/ParticleVariable.h>
#include <Uintah/Grid/ReductionVariable.h>
#include <Uintah/Grid/Task.h>
#include <Uintah/Grid/VarLabel.h>
#include <SCICore/Math/MinMax.h>
#include <Uintah/Components/MPM/Util/Matrix3.h>
#include <Uintah/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Uintah/Grid/VarTypes.h>
#include <Uintah/Components/MPM/MPMLabel.h>
#include <SCICore/Malloc/Allocator.h>
#include <fstream>
#include <iostream>

using std::cerr;
using namespace Uintah::MPM;
using SCICore::Math::Min;
using SCICore::Math::Max;
using SCICore::Geometry::Vector;

ViscoScram::ViscoScram(ProblemSpecP& ps)
{
  ps->require("PR",d_initialData.PR);
  ps->require("CrackParameterA",d_initialData.CrackParameterA);
  ps->require("CrackPowerValue",d_initialData.CrackPowerValue);
  ps->require("CrackMaxGrowthRate",d_initialData.CrackMaxGrowthRate);
  ps->require("StressIntensityF",d_initialData.StressIntensityF);
  ps->require("CrackFriction",d_initialData.CrackFriction);
  ps->require("InitialCrackRadius",d_initialData.InitialCrackRadius);
  ps->require("CrackGrowthRate",d_initialData.CrackGrowthRate);
  ps->require("G1",d_initialData.G1);
  ps->require("G2",d_initialData.G2);
  ps->require("G3",d_initialData.G3);
  ps->require("G4",d_initialData.G4);
  ps->require("G5",d_initialData.G5);
  ps->require("RTau1",d_initialData.RTau1);
  ps->require("RTau2",d_initialData.RTau2);
  ps->require("RTau3",d_initialData.RTau3);
  ps->require("RTau4",d_initialData.RTau4);
  ps->require("RTau5",d_initialData.RTau5);
  ps->require("Beta",d_initialData.Beta);
  ps->require("Gamma",d_initialData.Gamma);
  ps->require("DCp_DTemperature",d_initialData.DCp_DTemperature);
  ps->require("LoadCurveNumber",d_initialData.LoadCurveNumber);
  ps->require("NumberOfPoints",d_initialData.NumberOfPoints);


//  p_cmdata_label = scinew VarLabel("p.cmdata",
//                                ParticleVariable<CMData>::getTypeDescription());
//  p_cmdata_label_preReloc = scinew VarLabel("p.cmdata+",
//                                ParticleVariable<CMData>::getTypeDescription());

  p_statedata_label = scinew VarLabel("p.statedata",
                                ParticleVariable<StateData>::getTypeDescription());
  p_statedata_label_preReloc = scinew VarLabel("p.statedata+",
                                ParticleVariable<StateData>::getTypeDescription());

//  bElBarLabel = scinew VarLabel("p.bElBar",
//                ParticleVariable<Matrix3>::getTypeDescription());
 
//  bElBarLabel_preReloc = scinew VarLabel("p.bElBar+",
//                ParticleVariable<Matrix3>::getTypeDescription());
}

ViscoScram::~ViscoScram()
{
  // Destructor

//  delete p_cmdata_label;
//  delete p_cmdata_label_preReloc;
  delete p_statedata_label;
  delete p_statedata_label_preReloc;
//  delete bElBarLabel;
//  delete bElBarLabel_preReloc;
 
}

void ViscoScram::initializeCMData(const Patch* patch,
                                        const MPMMaterial* matl,
                                        DataWarehouseP& new_dw)
{
   // Put stuff in here to initialize each particle's
   // constitutive model parameters and deformationMeasure
   Matrix3 Identity, zero(0.);
   Identity.Identity();

   ParticleSubset* pset = new_dw->getParticleSubset(matl->getDWIndex(), patch);
//   ParticleVariable<CMData> cmdata;
//   new_dw->allocate(cmdata, p_cmdata_label, pset);

   ParticleVariable<StateData> statedata;
   new_dw->allocate(statedata, p_statedata_label, pset);
   ParticleVariable<Matrix3> deformationGradient;
   new_dw->allocate(deformationGradient, lb->pDeformationMeasureLabel, pset);
   ParticleVariable<Matrix3> pstress;
   new_dw->allocate(pstress, lb->pStressLabel, pset);
//   ParticleVariable<Matrix3> bElBar;
//   new_dw->allocate(bElBar,  bElBarLabel, pset);

   for(ParticleSubset::iterator iter = pset->begin();
          iter != pset->end(); iter++) {
          statedata[*iter].VolumeChangeHeating = 0.0;
          statedata[*iter].ViscousHeating = 0.0;
          statedata[*iter].CrackHeating = 0.0;
          statedata[*iter].CrackRadius = d_initialData.InitialCrackRadius;
          statedata[*iter].DevStress1 = zero;
          statedata[*iter].DevStress2 = zero;
          statedata[*iter].DevStress3 = zero;
          statedata[*iter].DevStress4 = zero;
          statedata[*iter].DevStress5 = zero;

          deformationGradient[*iter] = Identity;
//          bElBar[*iter] = Identity;
          pstress[*iter] = zero;
   }
   new_dw->put(statedata, p_statedata_label);
   new_dw->put(deformationGradient, lb->pDeformationMeasureLabel);
   new_dw->put(pstress, lb->pStressLabel);
//   new_dw->put(bElBar, bElBarLabel);

   computeStableTimestep(patch, matl, new_dw);

}

void ViscoScram::addParticleState(std::vector<const VarLabel*>& from,
				   std::vector<const VarLabel*>& to)
{
   from.push_back(p_statedata_label);
   to.push_back(p_statedata_label_preReloc);
//   from.push_back(bElBarLabel);
//   to.push_back(bElBarLabel_preReloc);
}

void ViscoScram::computeStableTimestep(const Patch* patch,
                                           const MPMMaterial* matl,
                                           DataWarehouseP& new_dw)
{
   // This is only called for the initial timestep - all other timesteps
   // are computed as a side-effect of computeStressTensor
  Vector dx = patch->dCell();
  int matlindex = matl->getDWIndex();
  // Retrieve the array of constitutive parameters
  ParticleSubset* pset = new_dw->getParticleSubset(matlindex, patch);
  ParticleVariable<StateData> statedata;
  new_dw->get(statedata, p_statedata_label, pset);
  ParticleVariable<double> pmass;
  new_dw->get(pmass, lb->pMassLabel, pset);
  ParticleVariable<double> pvolume;
  new_dw->get(pvolume, lb->pVolumeLabel, pset);
  ParticleVariable<Vector> pvelocity;
  new_dw->get(pvelocity, lb->pVelocityLabel, pset);

  double c_dil = 0.0;
  Vector WaveSpeed(1.e-12,1.e-12,1.e-12);

  double G = d_initialData.G1 + d_initialData.G2 +
 	      d_initialData.G3 + d_initialData.G4 + d_initialData.G5;
  double bulk = (2.*G*(1. + d_initialData.PR))/(3.*(1.-2.*d_initialData.PR));
  for(ParticleSubset::iterator iter = pset->begin();
      iter != pset->end(); iter++){
     particleIndex idx = *iter;

     // Compute wave speed at each particle, store the maximum
     c_dil = sqrt((bulk + 4.*G/3.)*pvolume[idx]/pmass[idx]);
     WaveSpeed=Vector(Max(c_dil+fabs(pvelocity[idx].x()),WaveSpeed.x()),
		      Max(c_dil+fabs(pvelocity[idx].y()),WaveSpeed.y()),
		      Max(c_dil+fabs(pvelocity[idx].z()),WaveSpeed.z()));
    }
    WaveSpeed = dx/WaveSpeed;
    double delT_new = WaveSpeed.minComponent();
    new_dw->put(delt_vartype(delT_new), lb->delTLabel);
}

void ViscoScram::computeStressTensor(const Patch* patch,
                                        const MPMMaterial* matl,
                                        DataWarehouseP& old_dw,
                                        DataWarehouseP& new_dw)
{
  Matrix3 velGrad,Shear,fbar,deformationGradientInc;
  double J,IEl,muBar,U,W,se=0.;
  double c_dil=0.0,Jinc;
  Vector WaveSpeed(1.e-12,1.e-12,1.e-12);
  double onethird = (1.0/3.0);
  Matrix3 Identity;
  double sqrtopf=sqrt(1.5);
  double PI = 3.141592654;

  Identity.Identity();

  Vector dx = patch->dCell();
  double oodx[3] = {1./dx.x(), 1./dx.y(), 1./dx.z()};

  int matlindex = matl->getDWIndex();
  // Create array for the particle position
  ParticleSubset* pset = old_dw->getParticleSubset(matlindex, patch);
  ParticleVariable<Point> px;
  old_dw->get(px, lb->pXLabel, pset);

  // Create array for the particle deformation
  ParticleVariable<Matrix3> deformationGradient;
  old_dw->get(deformationGradient, lb->pDeformationMeasureLabel, pset);
//  ParticleVariable<Matrix3> bElBar;
//  old_dw->get(bElBar, bElBarLabel, pset);

  // Create array for the particle stress
  ParticleVariable<Matrix3> pstress;
  old_dw->get(pstress, lb->pStressLabel, pset);

  // Retrieve the array of constitutive parameters
  ParticleVariable<StateData> statedata;
  old_dw->get(statedata, p_statedata_label, pset);
  ParticleVariable<double> pmass;
  old_dw->get(pmass, lb->pMassLabel, pset);
  ParticleVariable<double> pvolume;
  old_dw->get(pvolume, lb->pVolumeLabel, pset);
  ParticleVariable<Vector> pvelocity;
  old_dw->get(pvelocity, lb->pVelocityLabel, pset);

  NCVariable<Vector> gvelocity;

  new_dw->get(gvelocity, lb->gMomExedVelocityLabel, matlindex,patch,
            Ghost::AroundCells, 1);
  delt_vartype delT;
  old_dw->get(delT, lb->delTLabel);

  for(ParticleSubset::iterator iter = pset->begin();
     iter != pset->end(); iter++){
     particleIndex idx = *iter;

     velGrad.set(0.0);
     // Get the node indices that surround the cell
     IntVector ni[8];
     Vector d_S[8];
     if(!patch->findCellAndShapeDerivatives(px[idx], ni, d_S))
         continue;

      for(int k = 0; k < 8; k++) {
          Vector& gvel = gvelocity[ni[k]];
          for (int j = 0; j<3; j++){
            for (int i = 0; i<3; i++) {
                velGrad(i+1,j+1)+=gvel(i) * d_S[k](j) * oodx[j];
            }
          }
      }
    
      Matrix3 D = velGrad + velGrad.Transpose();

      // Calculate the stress Tensor (symmetric 3 x 3 Matrix) given the
      // time step and the velocity gradient and the material constants
      double G = d_initialData.G1 + d_initialData.G2 +
 	         d_initialData.G3 + d_initialData.G4 + d_initialData.G5;
      Matrix3 DPrime = D - Identity*onethird*D.Trace();
      double EDeff = sqrtopf*DPrime.Norm();
      Matrix3 DevStress = statedata[idx].DevStress1 + statedata[idx].DevStress2 
	                + statedata[idx].DevStress3 + statedata[idx].DevStress4 
			+ statedata[idx].DevStress5;
      double EffStress = sqrtopf*pstress[idx].Norm();
      double DevStressNorm = DevStress.Norm();
      double EffDevStress = sqrtopf*DevStressNorm;

      // Add code here to get vres from a lookup table
      double vres = 1.0;

      double p = -onethird * pstress[idx].Trace();

      int compflag = 0;
      if(p < 0.0){
	compflag = 1;
      }
      EffStress = (1+compflag)*EffDevStress - compflag*EffStress;
      vres = ((1 + compflag) - d_initialData.CrackGrowthRate*compflag)*vres;
      double sigmae = sqrt(DevStressNorm*DevStressNorm - compflag*(3*p*p));
      double sif = sqrt(3*PI*statedata[idx].CrackRadius/2)*sigmae;
      double cf = d_initialData.CrackFriction;
      double xmup = (1 + compflag)*sqrt(45./(2.*(3. - 2.*cf*cf)))*cf;
      double a = xmup*p*sqrt(statedata[idx].CrackRadius);
      double b = 1. + a/d_initialData.StressIntensityF;
      double termm = 1. + PI*a*b/d_initialData.StressIntensityF;
      double rko = d_initialData.StressIntensityF*sqrt(termm);
      double skp = rko*sqrt(1. + (2./d_initialData.CrackPowerValue));
      double sk1 = skp*pow((1. + (2./d_initialData.CrackPowerValue)),
			1./d_initialData.CrackPowerValue);
      double shear = d_initialData.InitialCrackRadius;
      double bulk  = d_initialData.InitialCrackRadius;

      // Compute the deformation gradient increment using the time_step
      // velocity gradient
      // F_n^np1 = dudx * dt + Identity
      deformationGradientInc = velGrad * delT + Identity;

      Jinc = deformationGradientInc.Determinant();

      // Update the deformation gradient tensor to its time n+1 value.
      deformationGradient[idx] = deformationGradientInc *
                             deformationGradient[idx];

    // get the volume preserving part of the deformation gradient increment
    fbar = deformationGradientInc * pow(Jinc,-onethird);

    IEl = 1.0;

    // Shear is equal to the shear modulus times dev(bElBar)
    Shear = Identity;

    // get the volumetric part of the deformation
    J = deformationGradient[idx].Determinant();

    // get the hydrostatic part of the stress
    p = 0.5*bulk*(J - 1.0/J);

    // compute the total stress (volumetric + deviatoric)
    pstress[idx] = Identity*p + Shear/J;

    // Compute the strain energy for all the particles
    U = .5*bulk*(.5*(pow(J,2.0) - 1.0) - log(J));
    W = .5;

    pvolume[idx]=Jinc*pvolume[idx];

    se += (U + W)*pvolume[idx]/J;

    // Compute wave speed at each particle, store the maximum
    muBar = IEl * shear;

    if(pmass[idx] > 0){
      c_dil = sqrt((bulk + 4.*shear/3.)*pvolume[idx]/pmass[idx]);
    }
    else{
      c_dil = 0.0;
      pvelocity[idx] = Vector(0.0,0.0,0.0);
    }
    WaveSpeed=Vector(Max(c_dil+fabs(pvelocity[idx].x()),WaveSpeed.x()),
		     Max(c_dil+fabs(pvelocity[idx].y()),WaveSpeed.y()),
		     Max(c_dil+fabs(pvelocity[idx].z()),WaveSpeed.z()));
  }

  WaveSpeed = dx/WaveSpeed;
  double delT_new = WaveSpeed.minComponent();
  new_dw->put(delt_vartype(delT_new), lb->delTLabel);
  new_dw->put(pstress, lb->pStressLabel_preReloc);
  new_dw->put(deformationGradient, lb->pDeformationMeasureLabel_preReloc);
//  new_dw->put(bElBar, bElBarLabel_preReloc);

  // Put the strain energy in the data warehouse
  new_dw->put(sum_vartype(se), lb->StrainEnergyLabel);

  // This is just carried forward
  new_dw->put(statedata, p_statedata_label_preReloc);
  // Store deformed volume
  new_dw->put(pvolume,lb->pVolumeDeformedLabel);
}

double ViscoScram::computeStrainEnergy(const Patch* patch,
                                        const MPMMaterial* matl,
                                        DataWarehouseP& new_dw)
{
  double se=0;

  return se;
}

void ViscoScram::addComputesAndRequires(Task* task,
					 const MPMMaterial* matl,
					 const Patch* patch,
					 DataWarehouseP& old_dw,
					 DataWarehouseP& new_dw) const
{
   task->requires(old_dw, lb->pXLabel, matl->getDWIndex(), patch,
                  Ghost::None);
   task->requires(old_dw, lb->pDeformationMeasureLabel, matl->getDWIndex(), patch,
                  Ghost::None);
   task->requires(old_dw, p_statedata_label, matl->getDWIndex(),  patch,
                  Ghost::None);
   task->requires(old_dw, lb->pMassLabel, matl->getDWIndex(),  patch,
                  Ghost::None);
   task->requires(old_dw, lb->pVolumeLabel, matl->getDWIndex(),  patch,
                  Ghost::None);
   task->requires(new_dw, lb->gMomExedVelocityLabel, matl->getDWIndex(), patch,
                  Ghost::AroundCells, 1);
   task->requires(old_dw, lb->delTLabel);

   task->computes(new_dw, lb->pStressLabel_preReloc, matl->getDWIndex(),  patch);
   task->computes(new_dw, lb->pDeformationMeasureLabel_preReloc, matl->getDWIndex(), patch);
   task->computes(new_dw, p_statedata_label_preReloc, matl->getDWIndex(),  patch);
   task->computes(new_dw, lb->pVolumeDeformedLabel, matl->getDWIndex(), patch);
}

#ifdef __sgi
#define IRIX
#pragma set woff 1209
#endif

namespace Uintah {
   namespace MPM {

static MPI_Datatype makeMPI_CMData()
{
   ASSERTEQ(sizeof(ViscoScram::StateData), sizeof(double)*2);
   MPI_Datatype mpitype;
   MPI_Type_vector(1, 2, 2, MPI_DOUBLE, &mpitype);
   return mpitype;
}

const TypeDescription* fun_getTypeDescription(ViscoScram::StateData*)
{
   static TypeDescription* td = 0;
   if(!td){
      td = scinew TypeDescription(TypeDescription::Other,
			       "ViscoScram::StateData", true, &makeMPI_CMData);
   }
   return td;
}
   }
}

// $Log$
// Revision 1.3  2000/08/21 23:13:54  guilkey
// Adding actual ViscoScram functionality.  Not done yet, but compiles.
//
// Revision 1.2  2000/08/21 19:01:37  guilkey
// Removed some garbage from the constitutive models.
//
// Revision 1.1  2000/08/21 18:37:41  guilkey
// Initial commit of ViscoScram stuff.  Don't get too excited yet,
// currently these are just cosmetically modified copies of CompNeoHook.
//
