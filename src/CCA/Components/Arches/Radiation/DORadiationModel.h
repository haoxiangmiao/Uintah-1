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

//----- DORadiationModel.h --------------------------------------------------

#ifndef Uintah_Component_Arches_DORadiationModel_h
#define Uintah_Component_Arches_DORadiationModel_h

/***************************************************************************
CLASS
    DORadiationModel
       Sets up the DORadiationModel
       
GENERAL INFORMATION
    DORadiationModel.h - Declaration of DORadiationModel class

    Author:Gautham Krishnamoorthy (gautham@crsim.utah.edu) 
           Rajesh Rawat (rawat@crsim.utah.edu)
    
    Creation Date : 06-18-2002

    C-SAFE
    
    
***************************************************************************/
#include <CCA/Components/Arches/Radiation/RadiationSolver.h>
#include <CCA/Components/Arches/TimeIntegratorLabel.h>
#include <CCA/Components/Arches/ArchesVariables.h>
#include <CCA/Components/Arches/ArchesConstVariables.h>
#include <CCA/Components/Arches/Arches.h>
#include <CCA/Ports/SchedulerP.h>
#include <CCA/Ports/DataWarehouseP.h>
#include <Core/Grid/LevelP.h>
#include <Core/Grid/Patch.h>
#include <Core/Grid/Variables/VarLabel.h>

namespace Uintah {

  class ArchesLabel;
  class BoundaryCondition;

class DORadiationModel{

public:

      RadiationSolver* d_linearSolver;

      DORadiationModel(const ArchesLabel* label,
                       const MPMArchesLabel* MAlab,
                       BoundaryCondition* bndry_cond, 
                       const ProcessorGroup* myworld);


      virtual ~DORadiationModel();


      virtual void problemSetup(ProblemSpecP& params);

      virtual void boundarycondition(const ProcessorGroup* pc,
                                     const Patch* patch,
                                     CellInformation* cellinfo, 
                                     ArchesVariables* vars,
                                     ArchesConstVariables* constvars);

      virtual void intensitysolve(const ProcessorGroup* pc,
                                  const Patch* patch,
                                  CellInformation* cellinfo, 
                                  ArchesVariables* vars,
                                  ArchesConstVariables* constvars, 
                                  CCVariable<double>& divQ,
                                  int wall_type);

private:

      double d_opl; // optical length
      const ArchesLabel*    d_lab;
      const MPMArchesLabel* d_MAlab;
      BoundaryCondition* d_boundaryCondition;
      const ProcessorGroup* d_myworld;
      const PatchSet* d_perproc_patches;
      
      int d_sn, d_totalOrds; // totalOrdinates = sn*(sn+2)

      void computeOrdinatesOPL();
      
      int d_lambda;
      int ffield;

      double d_wall_abskg; 
      double d_intrusion_abskg; 

      OffsetArray1<double> fraction;

      OffsetArray1<double> oxi;
      OffsetArray1<double> omu;
      OffsetArray1<double> oeta;
      OffsetArray1<double> wt;

      OffsetArray1<double> rgamma;
      OffsetArray1<double> sd15;
      OffsetArray1<double> sd;
      OffsetArray1<double> sd7;
      OffsetArray1<double> sd3;

      OffsetArray1<double> srcbm;
      OffsetArray1<double> srcpone;
      OffsetArray1<double> qfluxbbm;

      bool d_print_all_info; 

}; // end class RadiationModel

} // end namespace Uintah

#endif