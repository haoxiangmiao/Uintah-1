//
// $Id$
//

#ifndef Uintah_Components_Arches_RBGSSolver_h
#define Uintah_Components_Arches_RBGSSolver_h

/**************************************
CLASS
   RBGSSolver
   
   Class RBGSSolver is a point red-black Gauss-Seidel
   solver

GENERAL INFORMATION
   RBGSSolver.h - declaration of the class
   
   Author: Rajesh Rawat (rawat@crsim.utah.edu)
   
   Creation Date:   Mar 1, 2000
   
   C-SAFE 
   
   Copyright U of U 2000

KEYWORDS


DESCRIPTION
   Class RBGSSolver is a point red-black Gauss-Seidel
   solver




WARNING
none
****************************************/

#include <Uintah/Components/Arches/LinearSolver.h>
#include <Uintah/Parallel/ProcessorContext.h>
#include <Uintah/Interface/SchedulerP.h>
#include <Uintah/Interface/DataWarehouseP.h>
#include <Uintah/Grid/LevelP.h>
#include <Uintah/Grid/Patch.h>
#include <Uintah/Grid/VarLabel.h>

#include <SCICore/Containers/Array1.h>

namespace Uintah {
namespace ArchesSpace {
  //class LinearSolver;
using namespace SCICore::Containers;

class RBGSSolver: public LinearSolver {

public:

      // GROUP: Constructors:
      ////////////////////////////////////////////////////////////////////////
      //
      // Construct an instance of a RBGSSolver.
      //
      // PRECONDITIONS
      //
      // POSTCONDITIONS
      //
      // Default constructor.
      //
      RBGSSolver();

      // GROUP: Destructors:
      ////////////////////////////////////////////////////////////////////////
      //
      // Virtual Destructor
      //
      virtual ~RBGSSolver();

      // GROUP: Problem Setup:
      ////////////////////////////////////////////////////////////////////////
      //
      // Problem setup
      //
      void problemSetup(const ProblemSpecP& params);

      // GROUP:  Schedule Action :
      ////////////////////////////////////////////////////////////////////////
      //
      // Underrelaxation
      //
      void sched_underrelax(const LevelP& level,
			    SchedulerP& sched,
			    DataWarehouseP& old_dw,
			    DataWarehouseP& new_dw);

      ////////////////////////////////////////////////////////////////////////
      //
      // Pressure Solve
      //
      void sched_pressureSolve(const LevelP& level,
			       SchedulerP& sched,
			       DataWarehouseP& old_dw,
			       DataWarehouseP& new_dw);

      ////////////////////////////////////////////////////////////////////////
      //
      // Velocity Solve
      //
      void sched_velSolve(const LevelP& level,
			  SchedulerP& sched,
			  DataWarehouseP& old_dw,
			  DataWarehouseP& new_dw,
			  int index);

      ////////////////////////////////////////////////////////////////////////
      //
      // Scalar Solve
      //
      void sched_scalarSolve(const LevelP& level,
			     SchedulerP& sched,
			     DataWarehouseP& old_dw,
			     DataWarehouseP& new_dw,
			     int index);
protected:

private:

      // GROUP:  Actual Action :
      ////////////////////////////////////////////////////////////////////////
      //
      // Pressure Underrelaxation
      //
      void press_underrelax(const ProcessorContext* pc,
			    const Patch* patch,
			    DataWarehouseP& old_dw,
			    DataWarehouseP& new_dw);

      ////////////////////////////////////////////////////////////////////////
      //
      // Pressure Solve
      //
      void press_lisolve(const ProcessorContext* pc,
			 const Patch* patch,
			 DataWarehouseP& old_dw,
			 DataWarehouseP& new_dw);

      ////////////////////////////////////////////////////////////////////////
      //
      // Calculate pressure residuals
      //
      void press_residCalculation(const ProcessorContext* pc,
				  const Patch* patch,
				  DataWarehouseP& old_dw,
				  DataWarehouseP& new_dw);

      ////////////////////////////////////////////////////////////////////////
      //
      // Velocity Underrelaxation
      //
      void vel_underrelax(const ProcessorContext* pc,
			  const Patch* patch,
			  DataWarehouseP& old_dw,
			  DataWarehouseP& new_dw, 
			  int index);

      ////////////////////////////////////////////////////////////////////////
      //
      // Velocity Solve
      //
      void vel_lisolve(const ProcessorContext* pc,
		       const Patch* patch,
		       DataWarehouseP& old_dw,
		       DataWarehouseP& new_dw, 
		       int index);

      ////////////////////////////////////////////////////////////////////////
      //
      // Calculate Velocity residuals
      //
      void vel_residCalculation(const ProcessorContext* pc,
				const Patch* patch,
				DataWarehouseP& old_dw,
				DataWarehouseP& new_dw, 
				int index);

      ////////////////////////////////////////////////////////////////////////
      //
      // Scalar Underrelaxation
      //
      void scalar_underrelax(const ProcessorContext* pc,
			     const Patch* patch,
			     DataWarehouseP& old_dw,
			     DataWarehouseP& new_dw, 
			     int index);

      ////////////////////////////////////////////////////////////////////////
      //
      // Scalar Solve
      //
      void scalar_lisolve(const ProcessorContext* pc,
			  const Patch* patch,
			  DataWarehouseP& old_dw,
			  DataWarehouseP& new_dw, 
			  int index);

      ////////////////////////////////////////////////////////////////////////
      //
      // Calculate Scalar residuals
      //
      void scalar_residCalculation(const ProcessorContext* pc,
				   const Patch* patch,
				   DataWarehouseP& old_dw,
				   DataWarehouseP& new_dw, 
				   int index);

      int d_maxSweeps;
      double d_convgTol; // convergence tolerence
      double d_underrelax;
      double d_initResid;
      double d_residual;

      // const VarLabel *
      const VarLabel* d_pressureLabel;
      const VarLabel* d_presCoefLabel;
      const VarLabel* d_presNonLinSrcLabel;
      const VarLabel* d_presResidualLabel;

}; // End class RBGSSolver.h

} // End namespace ArchesSpace
} // End namespace Uintah

#endif  
  
//
// $Log$
// Revision 1.6  2000/06/04 22:40:15  bbanerje
// Added Cocoon stuff, changed task, require, compute, get, put arguments
// to reflect new declarations. Changed sub.mk to include all the new files.
//
//
