#ifndef UINTAH_MPMBOUNDCOND_H
#define UINTAH_MPMBOUNDCOND_H

#include <SCIRun/Core/Geometry/Vector.h>
#include <Core/Grid/Variables/NCVariable.h>

#include <CCA/Components/MPM/share.h>

namespace Uintah {

using namespace SCIRun;

  class SCISHARE MPMBoundCond {

  public:
    
    MPMBoundCond();
    ~MPMBoundCond();

    void setBoundaryCondition(const Patch* patch,int dwi, const string& type,
			      NCVariable<Vector>& variable,int n8or27=8);

    void setBoundaryCondition(const Patch* patch,int dwi, const string& type,
			      NCVariable<double>& variable,int n8or27=8);

    void setBoundaryCondition(const Patch* patch,int dwi, const string& type,
			      NCVariable<double>& variable,
			      constNCVariable<double>& gvolume,int n8or27=8);

  private:

  };



}

#endif
