#ifndef __CONTACT_H__
#define __CONTACT_H__

#include <Packages/Uintah/CCA/Ports/DataWarehouseP.h>
#include <Packages/Uintah/CCA/Components/MPM/MPMLabel.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/SimulationStateP.h>
#include <Packages/Uintah/Core/Grid/VarLabel.h>
#include <Packages/Uintah/Core/Grid/NCVariable.h>
#include <Packages/Uintah/Core/Grid/ReductionVariable.h>
#include <Packages/Uintah/Core/Grid/SimulationStateP.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Core/Geometry/Vector.h>
#include <Core/Math/MinMax.h>

#include <math.h>

namespace Uintah {
using namespace SCIRun;

   class ProcessorGroup;
   class Patch;
   class VarLabel;
   class Task;

/**************************************

CLASS
   Contact
   
   Short description...

GENERAL INFORMATION

   Contact.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   Contact_Model

DESCRIPTION
   Long description...
  
WARNING

****************************************/

      class Contact {
      public:
         // Constructor
	 Contact();
	 virtual ~Contact();

	 // Basic contact methods
	 virtual void exMomInterpolated(const ProcessorGroup*,
					const PatchSubset* patches,
					const MaterialSubset* matls,
					DataWarehouse* old_dw,
					DataWarehouse* new_dw) = 0;
	 
	 virtual void exMomIntegrated(const ProcessorGroup*,
				      const PatchSubset* patches,
				      const MaterialSubset* matls,
				      DataWarehouse* old_dw,
				      DataWarehouse* new_dw) = 0;

	 virtual void initializeContact(const Patch* patch,
					int vfindex,
					DataWarehouse* new_dw) = 0;

         virtual void addComputesAndRequiresInterpolated(Task* task,
					   const PatchSet* patches,
					   const MaterialSet* matls) const = 0;
	 
         virtual void addComputesAndRequiresIntegrated(Task* task,
					   const PatchSet* patches,
					   const MaterialSet* matls) const = 0;

      protected:
	 MPMLabel* lb;
      };
      
      inline bool compare(double num1, double num2) {
	    double EPSILON=1.e-16;
	    
	    return (fabs(num1-num2) <= EPSILON);
      }

} // End namespace Uintah

#endif // __CONTACT_H__
