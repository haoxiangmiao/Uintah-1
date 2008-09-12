
#ifndef Uintah_Components_Arches_Discretization_h
#define Uintah_Components_Arches_Discretization_h

//#include <Packages/Uintah/CCA/Components/Arches/StencilMatrix.h>
//#include <Packages/Uintah/Core/Grid/Variables/CCVariable.h>
//#include <Packages/Uintah/Core/Grid/FCVariable.h>
#include <Packages/Uintah/CCA/Ports/SchedulerP.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouseP.h>
#include <Packages/Uintah/Core/Grid/LevelP.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/Variables/VarLabel.h>
#include <Packages/Uintah/CCA/Components/Arches/Arches.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesVariables.h>
#include <Packages/Uintah/CCA/Components/Arches/ArchesConstVariables.h>
#ifdef PetscFilter
#include <Packages/Uintah/CCA/Components/Arches/Filter.h>
#endif

#include <Core/Containers/Array1.h>
namespace Uintah {
  class ProcessorGroup;

//class StencilMatrix;
using namespace SCIRun;

/**************************************

CLASS
   Discretization
   
   Class Discretization is a class
   that computes stencil weights for linearized 
   N-S equations.  

GENERAL INFORMATION
   Discretization.h - declaration of the class
   
   Author: Rajesh Rawat (rawat@crsim.utah.edu)

   All major modifications since 01.01.2004 done by:
   Stanislav Borodai(borodai@crsim.utah.edu)
   
   Creation Date:   Mar 1, 2000
   
   C-SAFE 
   
   Copyright U of U 2000

KEYWORDS


DESCRIPTION
   Class Discretization is an abstract base class
   that computes stencil weights for linearized 
   N-S equations.  

WARNING
   none

****************************************/

class Discretization {

public:

  // GROUP: Constructors:
  ////////////////////////////////////////////////////////////////////////
  // Construct an instance of a Discretization.
  // PRECONDITIONS
  // POSTCONDITIONS
  // Default constructor.
  Discretization();

  // GROUP: Destructors:
  ////////////////////////////////////////////////////////////////////////
  // Virtual Destructor
  virtual ~Discretization();

  // GROUP:  Action Methods
  ////////////////////////////////////////////////////////////////////////
  // Set stencil weights. (Velocity)
  // It uses second order hybrid differencing for computing
  // coefficients
  void calculateVelocityCoeff(const Patch* patch,
                              double delta_t,
                              int index, 
                              bool lcentral,
                              CellInformation* cellinfo,
                              ArchesVariables* vars,
                              ArchesConstVariables* constvars);

  ////////////////////////////////////////////////////////////////////////
  // Set stencil weights. (Scalars)
  // It uses second order hybrid differencing for computing
  // coefficients
  void calculateScalarCoeff(const Patch* patch,
                            CellInformation* cellinfo,
                            ArchesVariables* vars,
                            ArchesConstVariables* constvars,
                            int conv_scheme);
  template<class T>
  void compute_Ap(CellIterator iter,
                  CCVariable<Stencil7>& A,
                  T& source);
                  
  ////////////////////////////////////////////////////////////////////////
  // Documentation here
  void calculateVelDiagonal(const Patch* patch,
                            int index,
                            ArchesVariables* vars);

  ////////////////////////////////////////////////////////////////////////
  // Documentation here
  void calculatePressDiagonal(const Patch* patch, 
                              ArchesVariables* vars);

  ////////////////////////////////////////////////////////////////////////
  // Documentation here
  void calculateScalarDiagonal(const Patch* patch,
                               ArchesVariables* vars);
  ////////////////////////////////////////////////////////////////////////
  // Documentation here
  void calculateScalarFluxLimitedConvection(const Patch* patch,
                                            CellInformation* cellinfo,
                                            ArchesVariables* vars,
                                            ArchesConstVariables* constvars,
                                            const int wall_celltypeval,
                                            int limiter_type,
                                            int boundary_limiter_type,
                                            bool central_limiter);


  void computeDivergence(const ProcessorGroup*,
                         const Patch* patch,
                         DataWarehouse* new_dw,
                         ArchesVariables* vars,
                         ArchesConstVariables* constvars,
                         const bool filter_divergence,
                         const bool periodic);

#ifdef PetscFilter
  inline void setFilter(Filter* filter) {
    d_filter = filter;
  }
#endif
  inline void setTurbulentPrandtlNumber(double turbPrNo) {
    d_turbPrNo = turbPrNo;
  }

  inline void setMMS(bool doMMS) {
    d_doMMS=doMMS;
  }
  inline bool getMMS() const {
    return d_doMMS;
  }
protected:

private:
   
      // Stencil weights.
      // Array of size NDIM and of depth determined by stencil coefficients
      //StencilMatrix<CCVariable<double> >* d_press_stencil_matrix;
      // stores coefficients for all the velocity components
      // coefficients should be saved on staggered grid
      //StencilMatrix<FCVariable<double> >* d_mom_stencil_matrix;
      // coefficients for all the scalar components
      //StencilMatrix<CCVariable<double> >* d_scalar_stencil_matrix;

#ifdef PetscFilter
      Filter* d_filter;
#endif
      double d_turbPrNo;
      bool d_doMMS;
}; // end class Discretization

} // End namespace Uintah

#endif  
  
