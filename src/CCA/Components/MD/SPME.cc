/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
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
//---------------------------------------------------------------------------------------------------------------------------------
#include <CCA/Components/MD/CenteredCardinalBSpline.h>
#include <CCA/Components/MD/MapPoint.h>
#include <CCA/Components/MD/MDSystem.h>
#include <CCA/Components/MD/SPME.h>
#include <CCA/Components/MD/SimpleGrid.h>
#include <Core/Grid/Box.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/Variables/ParticleVariable.h>
#include <Core/Grid/Variables/CCVariable.h>
#include <Core/Grid/Variables/CellIterator.h>
#include <Core/Math/MiscMath.h>

#include <iostream>
#include <complex>

#include <sci_values.h>
#include <sci_defs/fftw_defs.h>

using namespace Uintah;
using namespace SCIRun;

SPME::SPME()
{

}

SPME::~SPME()
{
delete}

SPME::SPME(const MDSystem* system,
           const double ewaldBeta,
           const bool isPolarizable,
           const double tolerance,
           const SCIRun::IntVector& kLimits,
           const int splineOrder) :
    EwaldBeta(ewaldBeta), polarizable(isPolarizable), PolarizationTolerance(tolerance), KLimits(kLimits)
{
  InterpolatingSpline = scinew CenteredCardinalBSpline(splineOrder);
  ElectrostaticMethod = Electrostatics::SPME;

  // Initialize and check for proper construction
  this->initialize(system);
  this->setup();
  SCIRun::IntVector localGridSize = localGridExtents + localGhostPositiveSize + localGhostNegativeSize;
  SimpleGrid<complex<double> > Q(localGridSize, localGridOffset, localGhostNegativeSize, localGhostPositiveSize);
  Q.initialize(complex<double>(0.0, 0.0));
}

std::vector<dblcomplex> SPME::generateBVector(const std::vector<double>& MFractional,
                                              const int InitialVectorIndex,
                                              const int LocalGridExtent,
                                              const CenteredCardinalBSpline& InterpolatingSpline) const
{
  double PI = acos(-1.0);
  double TwoPI = 2.0 * PI;
  double orderM12PI = TwoPI * (InterpolatingSpline.Order() - 1);

  int HalfSupport = InterpolatingSpline.HalfSupport();
  std::vector<dblcomplex> b(LocalGridExtent);
  std::vector<double> ZeroAlignedSpline = InterpolatingSpline.Evaluate(0);

  double* LocalMFractional = MFractional[InitialVectorIndex];  // Reset MFractional zero so we can index into it negatively
  for (size_t Index = 0; Index < LocalGridExtent; ++Index) {
    double Internal = TwoPI * LocalMFractional[Index];
    // Formula looks significantly different from given SPME for offset splines.
    //   See Essmann et. al., J. Chem. Phys. 103 8577 (1995). for conversion, particularly formula C3 pt. 2 (paper uses pt. 4)
    dblcomplex Phi_N = 0.0;
    for (int DenomIndex = -HalfSupport; DenomIndex <= HalfSupport; ++DenomIndex) {
      Phi_N += dblcomplex(cos(Internal * DenomIndex), sin(Internal * DenomIndex));
    }
    b[Index] = 1.0 / Phi_N;
  }
  return b;
}

SimpleGrid<double> SPME::CalculateBGrid(const SCIRun::IntVector& localExtents,
                                        const SCIRun::IntVector& globalOffset) const
{

  size_t Limit_Kx = KLimits.x();
  size_t Limit_Ky = KLimits.y();
  size_t Limit_Kz = KLimits.z();

  std::vector<double> mf1 = SPME::generateMFractionalVector(Limit_Kx, InterpolatingSpline);
  std::vector<double> mf2 = SPME::generateMFractionalVector(Limit_Ky, InterpolatingSpline);
  std::vector<double> mf3 = SPME::generateMFractionalVector(Limit_Kz, InterpolatingSpline);

  // localExtents is without ghost grid points
  std::vector<dblcomplex> b1 = generateBVector(mf1, globalOffset.x(), localExtents.x(), InterpolatingSpline);
  std::vector<dblcomplex> b2 = generateBVector(mf2, globalOffset.y(), localExtents.y(), InterpolatingSpline);
  std::vector<dblcomplex> b3 = generateBVector(mf3, globalOffset.z(), localExtents.z(), InterpolatingSpline);

  SimpleGrid<double> BGrid(localExtents, globalOffset, 0);  // No ghost cells; internal only

  size_t XExtents = localExtents.x();
  size_t YExtents = localExtents.y();
  size_t ZExtents = localExtents.z();

  int XOffset = globalOffset.x();
  int YOffset = globalOffset.y();
  int ZOffset = globalOffset.z();

  for (size_t kX = 0; kX < XExtents; ++kX) {
    for (size_t kY = 0; kY < YExtents; ++kY) {
      for (size_t kZ = 0; kZ < ZExtents; ++kZ) {
        BGrid(kX, kY, kZ) = norm(b1[kX + XOffset]) * norm(b2[kY + YOffset]) * norm(b3[kZ + ZOffset]);
      }
    }
  }
  return BGrid;
}

SimpleGrid<double> SPME::CalculateCGrid(const SCIRun::IntVector& Extents,
                                        const SCIRun::IntVector& Offset) const
{

  std::vector<double> mp1 = SPME::generateMPrimeVector(KLimits.x(), InterpolatingSpline);
  std::vector<double> mp2 = SPME::generateMPrimeVector(KLimits.y(), InterpolatingSpline);
  std::vector<double> mp3 = SPME::generateMPrimeVector(KLimits.z(), InterpolatingSpline);

  size_t XExtents = Extents.x();
  size_t YExtents = Extents.y();
  size_t ZExtents = Extents.z();

  int XOffset = Offset.x();
  int YOffset = Offset.y();
  int ZOffset = Offset.z();

  double PI = acos(-1.0);
  double PI2 = PI * PI;
  double invBeta2 = 1.0 / (EwaldBeta * EwaldBeta);
  double invVolFactor = 1.0 / (SystemVolume * PI);

  SimpleGrid<double> CGrid(Extents, Offset, 0);  // No ghost cells; internal only
  for (size_t kX = 0; kX < XExtents; ++kX) {
    for (size_t kY = 0; kY < YExtents; ++kY) {
      for (size_t kZ = 0; kZ < ZExtents; ++kZ) {
        if (kX != 0 || kY != 0 || kZ != 0) {
          SCIRun::Vector m(mp1[kX + XOffset], mp2[kY + YOffset], mp3[kZ + ZOffset]);

          m *= InverseUnitCell;

          double M2 = m.length2();
          double factor = PI2 * M2 * invBeta2;
          CGrid(kX, kY, kZ) = invVolFactor * exp(-factor) / M2;
        }
      }
    }
  }
  CGrid(0, 0, 0) = 0;
  return CGrid;
}

SimpleGrid<Matrix3> SPME::CalculateStressPrefactor(const SCIRun::IntVector& Extents,
                                                   const SCIRun::IntVector& Offset)
{

  std::vector<double> mp1 = SPME::generateMPrimeVector(KLimits.x(), InterpolatingSpline);
  std::vector<double> mp2 = SPME::generateMPrimeVector(KLimits.y(), InterpolatingSpline);
  std::vector<double> mp3 = SPME::generateMPrimeVector(KLimits.z(), InterpolatingSpline);

  size_t XExtents = Extents.x();
  size_t YExtents = Extents.y();
  size_t ZExtents = Extents.z();

  int XOffset = Offset.x();
  int YOffset = Offset.y();
  int ZOffset = Offset.z();

  double PI = acos(-1.0);
  double PI2 = PI * PI;
  double invBeta2 = 1.0 / (EwaldBeta * EwaldBeta);

  SimpleGrid<Matrix3> StressPre(Extents, Offset, 0);  // No ghost cells; internal only
  for (size_t kX = 0; kX < XExtents; ++kX) {
    for (size_t kY = 0; kY < YExtents; ++kY) {
      for (size_t kZ = 0; kZ < ZExtents; ++kZ) {
        if (kX != 0 || kY != 0 || kZ != 0) {
          SCIRun::Vector m(mp1[kX + XOffset], mp2[kY + YOffset], mp3[kZ + ZOffset]);
          m *= InverseUnitCell;
          double M2 = m.length2();
          Matrix3 LocalStressContribution(-2.0 * (1.0 + PI2 * M2 * invBeta2) / M2);

          // Multiply by fourier vectorial contribution
          for (size_t s1 = 0; s1 < 3; ++s1) {
            for (size_t s2 = 0; s2 < 3; ++s2) {
              LocalStressContribution(s1, s2) *= (m[s1] * m[s2]);
            }
          }

          // Account for delta function
          for (size_t Delta = 0; Delta < 3; ++Delta) {
            LocalStressContribution(Delta, Delta) += 1.0;
          }

          StressPre(kX, kY, kZ) = LocalStressContribution;
        }
      }
    }
  }
  StressPre(0, 0, 0) = Matrix3(0);
  return StressPre;
}

// Interface implementations
void SPME::initialize(const MDSystem& SimulationSystem)
{
  // We call SPME::initialize from the constructor or if we've somehow maintained our object across a system change

  // Note:  I presume the indices are the local cell indices without ghost cells
  localGridExtents = patch->getCellHighIndex() - patch->getCellLowIndex();

  // Holds the index to map the local chunk of cells into the global cell structure
  localGridOffset = patch->getCellLowIndex();

  // Get useful information from global system descriptor to work with locally.
  UnitCell = SimulationSystem->UnitCell();
  InverseUnitCell = SimulationSystem->InverseUnitCell();
  SystemVolume = SimulationSystem->Volume();

  // Alan:  Not sure what the correct syntax is here, but the idea is that we'll store the number of ghost cells
  //          along each of the min/max boundaries.  This lets us differentiate should we need to for centered and
  //          left/right shifted splines
  localGhostPositiveSize = patch->getExtraCellHighIndex() - patch->getCellHighIndex();
  localGhostNegativeSize = patch->getCellLowIndex() - patch->getExtraCellLowIndex();
  return;
}

void SPME::setup()
{

  // We should only have to do this if KLimits or the inverse cell changes
  // Calculate B and C
  SimpleGrid<double> fBGrid = CalculateBGrid(localGridExtents, localGridOffset);
  SimpleGrid<double> fCGrid = CalculateCGrid(localGridExtents, localGridOffset);
  // Composite B and C into Theta
  size_t XExtent = localGridExtents.x();
  size_t YExtent = localGridExtents.y();
  size_t ZExtent = localGridExtents.z();
  for (size_t XIndex = 0; XIndex < XExtent; ++XIndex) {
    for (size_t YIndex = 0; YIndex < YExtent; ++YIndex) {
      for (size_t ZIndex = 0; ZIndex < ZExtent; ++ZIndex) {
        fTheta(XIndex, YIndex, ZIndex) = fBGrid(XIndex, YIndex, ZIndex) * fCGrid(XIndex, YIndex, ZIndex);
      }
    }
  }

  StressPrefactor = CalculateStressPrefactor
}

void SPME::calculate()
{
  // Note:  Must run SPME->setup() after every time there is a new box/K grid mapping (e.g. every step for NPT)
  //          This should be checked for in the system electrostatic driver
  vector<MapPoint> GridMap = SPME::GenerateChargeMap(pset, InterpolatingSpline);
  bool ElectrostaticsConverged = false;
  int NumberofIterations = 0;
  while (!ElectrostaticsConverged && (NumberofIterations < MaxIterations)) {
    SPME::MapChargeToGrid(GridMap, pset, InterpolatingSpline.HalfSupport());  // Calculate Q(r)

    // Map the local patch's charge grid into the global grid and transform
    SPME::GlobalMPIReduceChargeGrid(GHOST::AROUND);  //Ghost points should get transferred here
    SPME::ForwardTransformGlobalChargeGrid();  // Q(r) -> Q*(k)
    // Once reduced and transformed, we need the local grid re-populated with Q*(k)
    SPME::MPIDistributeLocalChargeGrid(GHOST::NONE);

    // Multiply the transformed Q out
    size_t XExtent = localGridExtents.x();
    size_t YExtent = localGridExtents.y();
    size_t ZExtent = localGridExtents.z();
    double localEnergy = 0.0;  //Maybe should be global?
    Matrix3 localStress(0.0);  //Maybe should be global?
    for (int kX = 0; kX < XExtent; ++kX) {
      for (int kY = 0; kY < YExtent; ++kY) {
        for (int kZ = 0; kZ < ZExtent; ++kZ) {
          complex<double> GridValue = Q(kX, kY, kZ);
          Q(kX, kY, kZ) = GridValue * conj(GridValue) * fTheta(kX, kY, kZ);  // Calculate (Q*Q^)*(B*C)
          localEnergy += Q(kX, kY, kZ);
          localStress += Q(kX, kY, kZ) * StressPrefactor(kX, kY, kZ);
        }
      }
    }

    // Transform back to real space
    SPME::GlobalMPIReduceChargeGrid(GHOST::NONE);  //Ghost points should NOT get transferred here
    SPME::ReverseTransformGlobalChargeGrid();
    SPME::MPIDistributeLocalChargeGrid(GHOST::AROUND);

    //  This may need to be before we transform the charge grid back to real space if we can calculate
    //    polarizability from the fourier space component
    ElectrostaticsConverged = true;
    if (polarizable) {
      // calculate polarization here
      // if (RMSPolarizationDifference > PolarizationTolerance) { ElectrostaticsConverged = false; }
      std::cerr << "Error:  Polarization not currently implemented!";
    }
    // Sanity check - Limit maximum number of polarization iterations we try
    ++NumberofIterations;
  }
  SPME::GlobalReduceEnergy();
  SPME::GlobalReduceStress();  //Uintah framework?

}

void SPME::finalize()
{
  SPME::MapForcesFromGrid(pset, ChargeMap);  // Calculate electrostatic contribution to f_ij(r)
  //Reduction for Energy, Pressure Tensor?
  // Something goes here, though I'm not sure what
  // Output?
}

std::vector<MapPoint> SPME::GenerateChargeMap(ParticleSubset* localParticleSet,
                                              CenteredCardinalBSpline& Spline)
{

  size_t MaxParticleIndex = localParticleSet->size();
  std::vector<MapPoint> ChargeMap;
  // Loop through particles
  for (size_t ChargeIndex = 0; ChargeIndex < MaxParticleIndex; ++ChargeIndex) {
    int ParticleID = localParticleSet[ChargeIndex]->GetParticleID();
    SCIRun::Vector ParticleGridCoordinates;

    //Calculate reduced coordinates of point to recast into charge grid
    ParticleGridCoordinates = ((localParticleSet[ChargeIndex]->GetParticleCoordinates()).AsVector()) * InverseUnitCell;
    // ** NOTE: JBH --> We may want to do this with a bit more thought eventually, since multiplying by the InverseUnitCell
    //                  is expensive if the system is orthorhombic, however it's not clear it's more expensive than dropping
    //                  to call MDSystem->IsOrthorhombic() and then branching the if statement appropriately.

    // This bit is tedious since we don't have any cross-pollination between type Vector and type IntVector.
    // Should we put that in (requires modifying Uintah framework).
    SCIRun::Vector KReal, SplineValues;
    SCIRun::IntVector ParticleGridOffset;
    for (size_t Index = 0; Index < 3; ++Index) {
      KReal[Index] = static_cast<double>(KLimits[Index]);  // For some reason I can't construct a Vector from an IntVector -- Maybe we should fix that instead?
      ParticleGridCoordinates[Index] *= KReal[Index];         // Recast particle into charge grid based representation
      ParticleGridOffset[Index] = static_cast<int>(ParticleGridCoordinates[Index]);  // Reference grid point for particle
      SplineValues[Index] = ParticleGridCoordinates[Index] - ParticleGridOffset[Index];  // Spline offset for spline function
    }
    vector<double> XSplineArray = Spline.Evaluate(SplineValues[0]);
    vector<double> YSplineArray = Spline.Evaluate(SplineValues[1]);
    vector<double> ZSplineArray = Spline.Evaluate(SplineValues[2]);

    vector<double> XSplineDeriv = Spline.Derivative(SplineValues[0]);
    vector<double> YSplineDeriv = Spline.Derivative(SplineValues[1]);
    vector<double> ZSplineDeriv = Spline.Derivative(SplineValues[2]);

    //MapPoint CurrentMapPoint(ParticleID,ParticleGridOffset,XSplineArray,YSplineArray,ZSplineArray);
    //

    SimpleGrid<double> ChargeGrid(XSplineArray, YSplineArray, ZSplineArray, ParticleGridOffset, 0);
    SimpleGrid<SCIRun::Vector> ForceGrid(XSplineDeriv.size(), YSplineDeriv.size(), ZSplineDeriv.size(), ParticleGridOffset, 0);
    size_t XExtent = XSplineArray.size();
    size_t YExtent = YSplineArray.size();
    size_t ZExtent = ZSplineArray.size();
    for (size_t XIndex = 0; XIndex < XExtent; ++XIndex) {
      for (size_t YIndex = 0; YIndex < YExtent; ++YIndex) {
        for (size_t ZIndex = 0; ZIndex < ZExtent; ++ZIndex) {
          ChargeGrid(XIndex, YIndex, ZIndex) = XSplineArray[XIndex] * YSplineArray[YIndex] * ZSplineArray[ZIndex];
          ForceGrid(XIndex, YIndex, ZIndex) = SCIRun::Vector(XSplineDeriv[XIndex], YSplineDeriv[YIndex], ZSplineDeriv[ZIndex]);
        }
      }
    }
    MapPoint CurrentMapPoint(ParticleID, ParticleGridOffset, ChargeGrid, ForceGrid);
    ChargeMap.push_back(CurrentMapPoint);
  }
  return ChargeMap;
}

void SPME::MapChargeToGrid(const std::vector<MapPoint>& GridMap,
                           const ParticleSubset* localParticleSubset,
                           int HalfSupport)
{
  size_t MaxParticleIndex = localParticleSubset->size();
  Q.initialize(0.0);  // Reset charges before we start adding onto them.
  for (size_t ParticleIndex = 0; ParticleIndex < MaxParticleIndex; ++ParticleIndex) {
    double Charge = localParticleSubset[ParticleIndex]->GetCharge();

    !FIXME
    Alan
    return (&ChargeGrid);
    SimpleGrid<double> ChargeMap = GridMap[ParticleIndex]->ChargeMapAddress();  //FIXME -- return reference, don't copy

    SCIRun::IntVector QAnchor = ChargeMap.getOffset();  // Location of the 0,0,0 origin for the charge map grid
    SCIRun::IntVector SupportExtent = ChargeMap.getExtents();  // Extents of the charge map grid
    for (int XMask = -HalfSupport; XMask <= HalfSupport; ++XMask) {
      for (int YMask = -HalfSupport; YMask <= HalfSupport; ++YMask) {
        for (int ZMask = -HalfSupport; ZMask <= HalfSupport; ++ZMask) {
          Q(QAnchor.x() + XMask, QAnchor.y() + YMask, QAnchor.z() + ZMask) += Charge
                                                                              * ChargeMap(XMask + HalfSupport, YMask + HalfSupport,
                                                                                          ZMask + HalfSupport);
        }
      }
    }

  }
}

void SPME::MapForceFromGrid(const std::vector<MapPoint>& GridMap,
                            const ParticleSubset* localParticleSubset,
                            int HalfSupport)
{
  size_t MaxParticleIndex = localParticleSubset->size();
  for (size_t ParticleIndex = 0; ParticleIndex < MaxParticleIndex; ++ParticleIndex) {
    SCIRun::Vector NewForce = localParticleSubset[ParticleIndex]->GetForce();
    SimpleGrid<SCIRun::Vector> ForceMap = GridMap[ParticleIndex]->ForceMapAddress();  // FIXME -- return reference, don't copy
    SCIRun::IntVector QAnchor = ForceMap.getOffset();  // Location of the 0,0,0 origin for the force map grid
    SCIRun::IntVector SupportExtent = ForceMap.getExtents();  // Extents of the force map grid
    for (int XMask = -HalfSupport; XMask <= HalfSupport; ++XMask) {
      for (int YMask = -HalfSupport; YMask <= HalfSupport; ++YMask) {
        for (int ZMask = -HalfSupport; ZMask <= HalfSupport; ++ZMask) {
          SCIRun::Vector CurrentForce;
          CurrentForce = ForceMap(XMask + HalfSupport, YMask + HalfSupport, ZMask + HalfSupport)
                         * Q(QAnchor.x() + XMask, QAnchor.y() + YMask, QAnchor.z() + ZMask);
          NewForce += CurrentForce;
        }
      }
    }
    localParticleSubset[ParticleIndex]->SetForce(NewForce);
  }
}

