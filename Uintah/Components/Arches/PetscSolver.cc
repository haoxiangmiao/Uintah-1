//----- PetscSolver.cc ----------------------------------------------

/* REFERENCED */
static char *id="@(#) $Id$";

#include <Uintah/Components/Arches/PetscSolver.h>
#include <Uintah/Components/Arches/PressureSolver.h>
#include <Uintah/Components/Arches/Discretization.h>
#include <Uintah/Components/Arches/Source.h>
#include <Uintah/Components/Arches/BoundaryCondition.h>
#include <Uintah/Components/Arches/TurbulenceModel.h>
#include <Uintah/Components/Arches/StencilMatrix.h>
#include <Uintah/Exceptions/InvalidValue.h>
#include <Uintah/Interface/Scheduler.h>
#include <Uintah/Interface/ProblemSpec.h>
#include <Uintah/Interface/DataWarehouse.h>
#include <Uintah/Grid/Level.h>
#include <Uintah/Grid/Task.h>
#include <Uintah/Grid/CCVariable.h>
#include <Uintah/Grid/SFCXVariable.h>
#include <Uintah/Grid/SFCYVariable.h>
#include <Uintah/Grid/SFCZVariable.h>
#include <SCICore/Util/NotFinished.h>
#include <Uintah/Components/Arches/Arches.h>
#include <Uintah/Components/Arches/ArchesFort.h>
#include <Uintah/Components/Arches/ArchesVariables.h>
#include <Uintah/Components/Arches/ArchesLabel.h>
#include <Uintah/Grid/VarTypes.h>
#include <Uintah/Grid/ReductionVariable.h>
#include <SCICore/Containers/Array1.h>
extern "C" {
#include "sles.h"
}
using namespace Uintah::ArchesSpace;
using namespace std;

// ****************************************************************************
// Default constructor for PetscSolver
// ****************************************************************************
PetscSolver::PetscSolver()
{
}

// ****************************************************************************
// Destructor
// ****************************************************************************
PetscSolver::~PetscSolver()
{
}

// ****************************************************************************
// Problem setup
// ****************************************************************************
void 
PetscSolver::problemSetup(const ProblemSpecP& params)
{
  ProblemSpecP db = params->findBlock("LinearSolver");
  db->require("max_iter", d_maxSweeps);
  db->require("res_tol", d_residual);
  db->require("underrelax", d_underrelax);
  int argc = 2;
  char** argv;
  argv = new char*[2];
  argv[0] = "PetscSolver::problemSetup";
  argv[1] = "-on_error_attach_debugger";
  int ierr = PetscInitialize(&argc, &argv, PETSC_NULL, PETSC_NULL);
  CHKERRQ(ierr);
}


// ****************************************************************************
// Actual compute of pressure residual
// ****************************************************************************
void 
PetscSolver::computePressResidual(const ProcessorGroup*,
				 const Patch* patch,
				 DataWarehouseP&,
				 DataWarehouseP&,
				 ArchesVariables* vars)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo = vars->pressure.getFortLowIndex();
  IntVector domHi = vars->pressure.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  //fortran call

  FORT_COMPUTERESID(domLo.get_pointer(), domHi.get_pointer(),
		    idxLo.get_pointer(), idxHi.get_pointer(),
		    vars->pressure.getPointer(),
		    vars->residualPressure.getPointer(),
		    vars->pressCoeff[Arches::AE].getPointer(), 
		    vars->pressCoeff[Arches::AW].getPointer(), 
		    vars->pressCoeff[Arches::AN].getPointer(), 
		    vars->pressCoeff[Arches::AS].getPointer(), 
		    vars->pressCoeff[Arches::AT].getPointer(), 
		    vars->pressCoeff[Arches::AB].getPointer(), 
		    vars->pressCoeff[Arches::AP].getPointer(), 
		    vars->pressNonlinearSrc.getPointer(),
		    &vars->residPress, &vars->truncPress);

#ifdef ARCHES_PRES_DEBUG
  cerr << " After Pressure Compute Residual : " << endl;
  for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
    cerr << "residual for ii = " << ii << endl;
    for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
      for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	cerr.width(14);
	cerr << vars->residualPressure[IntVector(ii,jj,kk)] << " " ; 
      }
      cerr << endl;
    }
  }
  cerr << "Resid Press = " << vars->residPress << " Trunc Press = " <<
    vars->truncPress << endl;
#endif
}


// ****************************************************************************
// Actual calculation of order of magnitude term for pressure equation
// ****************************************************************************
void 
PetscSolver::computePressOrderOfMagnitude(const ProcessorGroup* ,
				const Patch* ,
				DataWarehouseP& ,
				DataWarehouseP& , ArchesVariables* )
{

//&vars->truncPress

}

// ****************************************************************************
// Actual compute of pressure underrelaxation
// ****************************************************************************
void 
PetscSolver::computePressUnderrelax(const ProcessorGroup*,
				   const Patch* patch,
				   DataWarehouseP&,
				   DataWarehouseP&, 
				   ArchesVariables* vars)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo = vars->pressure.getFortLowIndex();
  IntVector domHi = vars->pressure.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  //fortran call
  FORT_UNDERELAX(domLo.get_pointer(), domHi.get_pointer(),
		 idxLo.get_pointer(), idxHi.get_pointer(),
		 vars->pressure.getPointer(),
		 vars->pressCoeff[Arches::AP].getPointer(), 
		 vars->pressNonlinearSrc.getPointer(), 
		 &d_underrelax);

#define ARCHES_PRES_DEBUG
#ifdef ARCHES_PRES_DEBUG
  cerr << " After Pressure Underrelax : " << endl;
  cerr << " Underrelaxation coefficient: " << d_underrelax << '\n';
  for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
    cerr << "pressure for ii = " << ii << endl;
    for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
      for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	cerr.width(14);
	cerr << vars->pressure[IntVector(ii,jj,kk)] << " " ; 
      }
      cerr << endl;
    }
  }
  cerr << " After Pressure Underrelax : " << endl;
  for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
    cerr << "pressure AP for ii = " << ii << endl;
    for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
      for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	cerr.width(14);
	cerr << (vars->pressCoeff[Arches::AP])[IntVector(ii,jj,kk)] << " " ; 
      }
      cerr << endl;
    }
  }
  cerr << " After Pressure Underrelax : " << endl;
  for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
    cerr << "pressure SU for ii = " << ii << endl;
    for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
      for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	cerr.width(14);
	cerr << vars->pressNonlinearSrc[IntVector(ii,jj,kk)] << " " ; 
      }
      cerr << endl;
    }
  }
#endif
#undef ARCHES_PRES_DEBUG
}

// ****************************************************************************
// Actual linear solve for pressure
// ****************************************************************************
void 
PetscSolver::pressLisolve(const ProcessorGroup* pc,
			 const Patch* patch,
			 DataWarehouseP& old_dw,
			 DataWarehouseP& new_dw,
			 ArchesVariables* vars,
			 const ArchesLabel* lab)
{
 
  // Get the patch bounds and the variable bounds
  IntVector domLo = vars->pressure.getFortLowIndex();
  IntVector domHi = vars->pressure.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  Vec x,b,u; // approx solution, RHS, exact solution
  Mat A; // linear system matrix
  SLES sles; // linear solver context
  KSP ksp;
  int ierr;
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
         Compute the matrix and right-hand-side vector that define
         the linear system, Ax = b.
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /* 
     Create parallel matrix, specifying only its global dimensions.
     When using MatCreate(), the matrix format can be specified at
     runtime. Also, the parallel partitioning of the matrix is
     determined by PETSc at runtime.

     Performance tuning note:  For problems of substantial size,
     preallocation of matrix memory is crucial for attaining good 
     performance.  Since preallocation is not possible via the generic
     matrix creation routine MatCreate(), we recommend for practical 
     problems instead to use the creation routine for a particular matrix
     format, e.g.,
         MatCreateMPIAIJ() - parallel AIJ (compressed sparse row)
         MatCreateMPIBAIJ() - parallel block AIJ
     See the matrix chapter of the users manual for details.
  */
  int numrows = (idxHi[0]-idxLo[0]+1)*(idxHi[1]-idxLo[1]+1)*
                (idxHi[2]-idxLo[2]+1);
  int nnx = idxHi[0]-idxLo[0]+1;
  int nny = idxHi[1]-idxLo[1]+1;
  int nnz = idxHi[2]-idxLo[2]+1;
  int numcolumns = numrows;
  int numnonzero = 7; // 7 point stencil
  ierr = MatCreateSeqAIJ(PETSC_COMM_WORLD, numrows, numcolumns, numnonzero,
			 PETSC_NULL, &A);
  CHKERRA(ierr);
  int row;
  int col[7];
  double value[7];
  // fill matrix where all 7 components are present
  for (row = nnx*nny; row < (nnz-1)*nnx*nny; row++) {
    col[0] = row-nnx*nny;
    col[1] = row-nnx;
    col[2] = row - 1;
    col[3] = row;
    col[4] = row + 1;
    col[5] = row+nnx;
    col[6] = row+nnx*nny;
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    value[0] = vars->pressCoeff[Arches::AB][IntVector(ii,jj,kk)];
    value[1] = vars->pressCoeff[Arches::AS][IntVector(ii,jj,kk)];
    value[2] = vars->pressCoeff[Arches::AW][IntVector(ii,jj,kk)];
    value[3] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
    value[4] = vars->pressCoeff[Arches::AE][IntVector(ii,jj,kk)];
    value[5] = vars->pressCoeff[Arches::AN][IntVector(ii,jj,kk)];
    value[6] = vars->pressCoeff[Arches::AT][IntVector(ii,jj,kk)];
    ierr = MatSetValues(A,1,&row,7,col,value,INSERT_VALUES);
    cerr << "set values: row=" << row << ", col=";
    for(int i=0;i<7;i++)
       cerr << col[i] << " ";
    cerr << ", value=";
    for(int i=0;i<7;i++)
       cerr << value[i] << " ";
    cerr << '\n';
    CHKERRA(ierr);
  }
  for (row = nnx; row < nny*nnx; row++) {
    col[0] = row-nnx;
    col[1] = row-1;
    col[2] = row;
    col[3] = row+1;
    col[4] = row+nnx;
    col[5] = row+nnx*nny;
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    value[0] = vars->pressCoeff[Arches::AS][IntVector(ii,jj,kk)];
    value[1] = vars->pressCoeff[Arches::AW][IntVector(ii,jj,kk)];
    value[2] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
    value[3] = vars->pressCoeff[Arches::AE][IntVector(ii,jj,kk)];
    value[4] = vars->pressCoeff[Arches::AN][IntVector(ii,jj,kk)];
    value[5] = vars->pressCoeff[Arches::AT][IntVector(ii,jj,kk)];
    ierr = MatSetValues(A,1,&row,6,col,value,INSERT_VALUES);
    cerr << "2. set values: row=" << row << ", col=";
    for(int i=0;i<6;i++)
       cerr << col[i] << " ";
    cerr << ", value=";
    for(int i=0;i<6;i++)
       cerr << value[i] << " ";
    cerr << '\n';
    CHKERRA(ierr);
  }
  for (row = nnx*nny*(nnz-1); row < (nnz-1)*nnx*nny+nnx*(nny-1); row++){
    col[0] = row-nnx*nny;
    col[1] = row-nnx;
    col[2] = row-1;
    col[3] = row;
    col[4] = row+1;
    col[5] = row+nnx;
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    value[0] = vars->pressCoeff[Arches::AB][IntVector(ii,jj,kk)];
    value[1] = vars->pressCoeff[Arches::AS][IntVector(ii,jj,kk)];
    value[2] = vars->pressCoeff[Arches::AW][IntVector(ii,jj,kk)];
    value[3] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
    value[4] = vars->pressCoeff[Arches::AE][IntVector(ii,jj,kk)];
    value[5] = vars->pressCoeff[Arches::AN][IntVector(ii,jj,kk)];
    ierr = MatSetValues(A,1,&row,6,col,value,INSERT_VALUES);
    cerr << "set values: row=" << row << ", col=";
    for(int i=0;i<6;i++)
       cerr << col[i] << " ";
    cerr << ", value=";
    for(int i=0;i<6;i++)
       cerr << value[i] << " ";
    cerr << '\n';
    CHKERRA(ierr);
  }
  for (row = 1; row < nnx; row++) {
    col[0] = row-1;
    col[1] = row;
    col[2] = row+1;
    col[3] = row+nnx;
    col[4] = row+nnx*nny;
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    value[0] = vars->pressCoeff[Arches::AW][IntVector(ii,jj,kk)];
    value[1] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
    value[2] = vars->pressCoeff[Arches::AE][IntVector(ii,jj,kk)];
    value[3] = vars->pressCoeff[Arches::AN][IntVector(ii,jj,kk)];
    value[4] = vars->pressCoeff[Arches::AT][IntVector(ii,jj,kk)];
    ierr = MatSetValues(A,1,&row,5,col,value,INSERT_VALUES);
    cerr << "set values: row=" << row << ", col=";
    for(int i=0;i<5;i++)
       cerr << col[i] << " ";
    cerr << ", value=";
    for(int i=0;i<5;i++)
       cerr << value[i] << " ";
    cerr << '\n';
    CHKERRA(ierr);
  }
  for (row = nnx*nny*(nnz-1)+nnx*(nny-1); 
       row < nnx*nny*(nnz-1)+nnx*(nny-1)+ nnx-1; row++) {
    col[0] = row-nnx*nny;
    col[1] = row-nnx;
    col[2] = row-1;
    col[3] = row;
    col[4] = row+1;
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    value[0] = vars->pressCoeff[Arches::AB][IntVector(ii,jj,kk)];
    value[1] = vars->pressCoeff[Arches::AS][IntVector(ii,jj,kk)];
    value[2] = vars->pressCoeff[Arches::AW][IntVector(ii,jj,kk)];
    value[3] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
    value[4] = vars->pressCoeff[Arches::AE][IntVector(ii,jj,kk)];
    ierr = MatSetValues(A,1,&row,5,col,value,INSERT_VALUES);
    cerr << "set values: row=" << row << ", col=";
    for(int i=0;i<5;i++)
       cerr << col[i] << " ";
    cerr << ", value=";
    for(int i=0;i<5;i++)
       cerr << value[i] << " ";
    cerr << '\n';
    CHKERRA(ierr);
  }
  row = 0;
  col[0] = row;
  col[1] = row+1;
  col[2] = row+nnx;
  col[3] = row+nnx*nny;
  int ii = 0;
  int jj = 0;
  int kk = 0;
  value[0] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
  value[1] = vars->pressCoeff[Arches::AE][IntVector(ii,jj,kk)];
  value[2] = vars->pressCoeff[Arches::AN][IntVector(ii,jj,kk)];
  value[3] = vars->pressCoeff[Arches::AT][IntVector(ii,jj,kk)];
  ierr = MatSetValues(A,1,&row,4,col,value,INSERT_VALUES);
  CHKERRA(ierr);
  row = nnx*nny*nnz-1;
  col[0] = row-nnx*nny;
  col[1] = row-nnx;
  col[2] = row-1;
  col[3] = row;
  ii = nnx-1;
  jj = nny-1;
  kk = nnz-1;
  value[0] = vars->pressCoeff[Arches::AB][IntVector(ii,jj,kk)];
  value[1] = vars->pressCoeff[Arches::AS][IntVector(ii,jj,kk)];
  value[2] = vars->pressCoeff[Arches::AW][IntVector(ii,jj,kk)];
  value[3] = vars->pressCoeff[Arches::AP][IntVector(ii,jj,kk)];
  ierr = MatSetValues(A,1,&row,4,col,value,INSERT_VALUES);
  CHKERRA(ierr);

  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRA(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRA(ierr);

  /* 
     Create vectors.  Note that we form 1 vector from scratch and
     then duplicate as needed.
  */
  ierr = VecCreate(PETSC_COMM_WORLD,PETSC_DECIDE,numrows,&x);CHKERRA(ierr);
  ierr = VecSetFromOptions(x);CHKERRA(ierr);
  ierr = VecDuplicate(x,&b);CHKERRA(ierr);
  ierr = VecDuplicate(x,&u);CHKERRA(ierr);
  // assemble right hand side and solution vector
  double vecvalueb, vecvaluex;
  for(row = 0; row < numrows; row++) {
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    vecvalueb = vars->pressNonlinearSrc[IntVector(ii,jj,kk)];
    cerr << "vecvalueb=" << vecvalueb << '\n';
    vecvaluex = vars->pressure[IntVector(ii,jj,kk)];
    VecSetValue(b, row, vecvalueb, INSERT_VALUES);
    VecSetValue(x, row, vecvaluex, INSERT_VALUES);
  }
  ierr = VecAssemblyBegin(b);CHKERRA(ierr);
  ierr = VecAssemblyEnd(b);CHKERRA(ierr);
  ierr = VecAssemblyBegin(x);CHKERRA(ierr);
  ierr = VecAssemblyEnd(x);CHKERRA(ierr);

  /* debugging - steve */
  ierr = ViewerSetFormat(VIEWER_STDOUT_WORLD, VIEWER_FORMAT_ASCII_DEFAULT, 0); CHKERRQ(ierr);
  double norm;
  ierr = MatNorm(A,NORM_1,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"matrix A norm = %g\n",norm);CHKERRQ(ierr);
  ierr = MatView(A, VIEWER_STDOUT_WORLD); CHKERRQ(ierr);

  ierr = VecNorm(x,NORM_1,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"vector x norm = %g\n",norm);CHKERRQ(ierr);
  ierr = VecView(x, VIEWER_STDOUT_WORLD); CHKERRQ(ierr);
  ierr = VecNorm(b,NORM_1,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"vector b norm = %g\n",norm);CHKERRQ(ierr);
  ierr = VecView(b, VIEWER_STDOUT_WORLD); CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                Create the linear solver and set various options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = SLESCreate(PETSC_COMM_WORLD,&sles);CHKERRA(ierr);
  ierr = SLESSetOperators(sles,A,A,DIFFERENT_NONZERO_PATTERN);CHKERRA(ierr);
  ierr = SLESGetKSP(sles,&ksp);CHKERRA(ierr);
  ierr = KSPSetInitialGuessNonzero(ksp);CHKERRA(ierr);
  ierr = SLESSetFromOptions(sles);CHKERRA(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                      Solve the linear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  int its;
  ierr = SLESSolve(sles,b,x,&its);CHKERRA(ierr);

  ierr = VecNorm(x,NORM_1,&norm);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"AFTER SOLVE vector x norm = %g\n",norm);CHKERRQ(ierr);
  ierr = VecView(x, VIEWER_STDOUT_WORLD); CHKERRQ(ierr);

  // check the error
  double neg_one = -1.0;
  ierr = MatMult(A, x, u);CHKERRA(ierr);
  ierr = VecAXPY(&neg_one, b, u); CHKERRA(ierr);
  ierr  = VecNorm(u,NORM_2,&norm);CHKERRA(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error %A, Iterations %d\n",norm,its);CHKERRA(ierr);
  // copy solution vector back into the array
  double* xvec;
  ierr = VecGetArray(x, &xvec); CHKERRQ(ierr);
  for(row = 0; row < numrows; row++) {
    int ii = ((row%(nnx*nny))%nnx);
    int jj = ((row - ii)%(nnx*nny))/nnx;
    int kk = (row-ii-nnx*jj)/(nnx*nny);
    // make it compatible with uintah's indices
    vars->pressure[IntVector(ii,jj,kk)] = xvec[row];
    cerr << "press" << IntVector(ii,jj,kk) << "=" << xvec[row] << '\n';
  }
  ierr = VecRestoreArray(x, &xvec); CHKERRQ(ierr);
  
  
/* 
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
  */
  ierr = SLESDestroy(sles);CHKERRA(ierr); 
  ierr = VecDestroy(u);CHKERRA(ierr);
  ierr = VecDestroy(b);CHKERRA(ierr);
  ierr = MatDestroy(A);CHKERRA(ierr);


 

#ifdef ARCHES_PRES_DEBUG
    cerr << "Iter # = " << pressIter << " Max Iters = " << d_maxSweeps 
	 << " Press. Resid = " << pressResid << " d_residual = " << d_residual
	 << " nlResid = " << nlResid << endl;
#endif
  // while((pressIter < d_maxSweeps)&&((pressResid > d_residual*nlResid)||
  //			      (pressResid > trunc_conv)));
#ifdef ARCHES_PRES_DEBUG
  cerr << "After pressure solve " << pressIter << " " << pressResid << endl;
  cerr << "After pressure solve " << nlResid << " " << trunc_conv <<  endl;
  cerr << " After Pressure solve : " << endl;
  for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
    cerr << "pressure for ii = " << ii << endl;
    for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
      for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	cerr.width(14);
	cerr << vars->pressure[IntVector(ii,jj,kk)] << " " ; 
      }
      cerr << endl;
    }
  }
#endif

  

}

// Shutdown PETSc
void PetscSolver::finalizeSolver()
{
  int ierr = PetscFinalize(); CHKERRQ(ierr);
}

//****************************************************************************
// Actual compute of Velocity residual
//****************************************************************************

void 
PetscSolver::computeVelResidual(const ProcessorGroup* ,
			       const Patch* patch,
			       DataWarehouseP& ,
			       DataWarehouseP& , 
			       int index, ArchesVariables* vars)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo;
  IntVector domHi;
  IntVector idxLo;
  IntVector idxHi;

  switch (index) {
  case Arches::XDIR:
    domLo = vars->uVelocity.getFortLowIndex();
    domHi = vars->uVelocity.getFortHighIndex();
    idxLo = patch->getSFCXFORTLowIndex();
    idxHi = patch->getSFCXFORTHighIndex();
    //fortran call

    FORT_COMPUTERESID(domLo.get_pointer(), domHi.get_pointer(),
		      idxLo.get_pointer(), idxHi.get_pointer(),
		      vars->uVelocity.getPointer(),
		      vars->residualUVelocity.getPointer(),
		      vars->uVelocityCoeff[Arches::AE].getPointer(), 
		      vars->uVelocityCoeff[Arches::AW].getPointer(), 
		      vars->uVelocityCoeff[Arches::AN].getPointer(), 
		      vars->uVelocityCoeff[Arches::AS].getPointer(), 
		      vars->uVelocityCoeff[Arches::AT].getPointer(), 
		      vars->uVelocityCoeff[Arches::AB].getPointer(), 
		      vars->uVelocityCoeff[Arches::AP].getPointer(), 
		      vars->uVelNonlinearSrc.getPointer(),
		      &vars->residUVel, &vars->truncUVel);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After U Velocity Compute Residual : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "u residual for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->residualUVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << "Resid U Vel = " << vars->residUVel << " Trunc U Vel = " <<
      vars->truncUVel << endl;
#endif

    break;
  case Arches::YDIR:
    domLo = vars->vVelocity.getFortLowIndex();
    domHi = vars->vVelocity.getFortHighIndex();
    idxLo = patch->getSFCYFORTLowIndex();
    idxHi = patch->getSFCYFORTHighIndex();
    //fortran call

    FORT_COMPUTERESID(domLo.get_pointer(), domHi.get_pointer(),
		      idxLo.get_pointer(), idxHi.get_pointer(),
		      vars->vVelocity.getPointer(),
		      vars->residualVVelocity.getPointer(),
		      vars->vVelocityCoeff[Arches::AE].getPointer(), 
		      vars->vVelocityCoeff[Arches::AW].getPointer(), 
		      vars->vVelocityCoeff[Arches::AN].getPointer(), 
		      vars->vVelocityCoeff[Arches::AS].getPointer(), 
		      vars->vVelocityCoeff[Arches::AT].getPointer(), 
		      vars->vVelocityCoeff[Arches::AB].getPointer(), 
		      vars->vVelocityCoeff[Arches::AP].getPointer(), 
		      vars->vVelNonlinearSrc.getPointer(),
		      &vars->residVVel, &vars->truncVVel);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After V Velocity Compute Residual : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "v residual for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->residualVVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << "Resid V Vel = " << vars->residVVel << " Trunc V Vel = " <<
      vars->truncVVel << endl;
#endif

    break;
  case Arches::ZDIR:
    domLo = vars->wVelocity.getFortLowIndex();
    domHi = vars->wVelocity.getFortHighIndex();
    idxLo = patch->getSFCYFORTLowIndex();
    idxHi = patch->getSFCYFORTHighIndex();
    //fortran call

    FORT_COMPUTERESID(domLo.get_pointer(), domHi.get_pointer(),
		      idxLo.get_pointer(), idxHi.get_pointer(),
		      vars->wVelocity.getPointer(),
		      vars->residualWVelocity.getPointer(),
		      vars->wVelocityCoeff[Arches::AE].getPointer(), 
		      vars->wVelocityCoeff[Arches::AW].getPointer(), 
		      vars->wVelocityCoeff[Arches::AN].getPointer(), 
		      vars->wVelocityCoeff[Arches::AS].getPointer(), 
		      vars->wVelocityCoeff[Arches::AT].getPointer(), 
		      vars->wVelocityCoeff[Arches::AB].getPointer(), 
		      vars->wVelocityCoeff[Arches::AP].getPointer(), 
		      vars->wVelNonlinearSrc.getPointer(),
		      &vars->residWVel, &vars->truncWVel);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After W Velocity Compute Residual : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "w residual for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->residualWVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << "Resid W Vel = " << vars->residWVel << " Trunc W Vel = " <<
      vars->truncWVel << endl;
#endif

    break;
  default:
    throw InvalidValue("Invalid index in LinearSolver for velocity");
  }
}


//****************************************************************************
// Actual calculation of order of magnitude term for Velocity equation
//****************************************************************************
void 
PetscSolver::computeVelOrderOfMagnitude(const ProcessorGroup* ,
				const Patch* ,
				DataWarehouseP& ,
				DataWarehouseP& , ArchesVariables* )
{

  //&vars->truncUVel
  //&vars->truncVVel
  //&vars->truncWVel

}



//****************************************************************************
// Velocity Underrelaxation
//****************************************************************************
void 
PetscSolver::computeVelUnderrelax(const ProcessorGroup* ,
				 const Patch* patch,
				 DataWarehouseP& ,
				 DataWarehouseP& , 
				 int index, ArchesVariables* vars)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo;
  IntVector domHi;
  IntVector idxLo;
  IntVector idxHi;

  switch (index) {
  case Arches::XDIR:
    domLo = vars->uVelocity.getFortLowIndex();
    domHi = vars->uVelocity.getFortHighIndex();
    idxLo = patch->getSFCXFORTLowIndex();
    idxHi = patch->getSFCXFORTHighIndex();
    FORT_UNDERELAX(domLo.get_pointer(), domHi.get_pointer(),
		   idxLo.get_pointer(), idxHi.get_pointer(),
		   vars->uVelocity.getPointer(),
		   vars->uVelocityCoeff[Arches::AP].getPointer(), 
		   vars->uVelNonlinearSrc.getPointer(),
		   &d_underrelax);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After U Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "U Vel for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->uVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << " After U Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "U Vel AP for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << (vars->uVelocityCoeff[Arches::AP])[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << " After U Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "U Vel SU for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->uVelNonlinearSrc[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif

    break;
    case Arches::YDIR:
    domLo = vars->vVelocity.getFortLowIndex();
    domHi = vars->vVelocity.getFortHighIndex();
    idxLo = patch->getSFCYFORTLowIndex();
    idxHi = patch->getSFCYFORTHighIndex();
    FORT_UNDERELAX(domLo.get_pointer(), domHi.get_pointer(),
		   idxLo.get_pointer(), idxHi.get_pointer(),
		   vars->vVelocity.getPointer(),
		   vars->vVelocityCoeff[Arches::AP].getPointer(), 
		   vars->vVelNonlinearSrc.getPointer(),
		   &d_underrelax);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After V Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "V Vel for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->vVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << " After V Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "V Vel AP for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << (vars->vVelocityCoeff[Arches::AP])[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << " After V Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "V Vel SU for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->vVelNonlinearSrc[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif

    break;
    case Arches::ZDIR:
    domLo = vars->wVelocity.getFortLowIndex();
    domHi = vars->wVelocity.getFortHighIndex();
    idxLo = patch->getSFCZFORTLowIndex();
    idxHi = patch->getSFCZFORTHighIndex();
    FORT_UNDERELAX(domLo.get_pointer(), domHi.get_pointer(),
		   idxLo.get_pointer(), idxHi.get_pointer(),
		   vars->wVelocity.getPointer(),
		   vars->wVelocityCoeff[Arches::AP].getPointer(), 
		   vars->wVelNonlinearSrc.getPointer(),
		   &d_underrelax);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After W Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "W Vel for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->wVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << " After W Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "W Vel AP for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << (vars->wVelocityCoeff[Arches::AP])[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
    cerr << " After W Vel Underrelax : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "W Vel SU for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->wVelNonlinearSrc[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif

    break;
  default:
    throw InvalidValue("Invalid index in LinearSolver for velocity");
  }
}


//****************************************************************************
// Velocity Solve
//****************************************************************************
void 
PetscSolver::velocityLisolve(const ProcessorGroup* pc,
			    const Patch* patch,
			    DataWarehouseP& old_dw ,
			    DataWarehouseP& new_dw, 
			    int index, double delta_t,
			    ArchesVariables* vars,
			    CellInformation* cellinfo,
			    const ArchesLabel* lab)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo;
  IntVector domHi;
  IntVector idxLo;
  IntVector idxHi;
  // for explicit solver
  IntVector domLoDen = vars->old_density.getFortLowIndex();
  IntVector domHiDen = vars->old_density.getFortHighIndex();
  
  IntVector Size;

  Array1<double> e1;
  Array1<double> f1;
  Array1<double> e2;
  Array1<double> f2;
  Array1<double> e3;
  Array1<double> f3;

  sum_vartype resid;
  sum_vartype trunc;

  double nlResid;
  double trunc_conv;

  int velIter = 0;
  double velResid = 0.0;
  double theta = 0.5;

  switch (index) {
  case Arches::XDIR:
    domLo = vars->uVelocity.getFortLowIndex();
    domHi = vars->uVelocity.getFortHighIndex();
    idxLo = patch->getSFCXFORTLowIndex();
    idxHi = patch->getSFCXFORTHighIndex();

    Size = domHi - domLo + IntVector(1,1,1);

    e1.resize(Size.x());
    f1.resize(Size.x());
    e2.resize(Size.y());
    f2.resize(Size.y());
    e3.resize(Size.z());
    f3.resize(Size.z());

    old_dw->get(resid, lab->d_uVelResidPSLabel);
    old_dw->get(trunc, lab->d_uVelTruncPSLabel);

    nlResid = resid;
    trunc_conv = trunc*1.0E-7;
#if implicit_defined
    do {
      //fortran call for lineGS solver
      FORT_LINEGS(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  vars->uVelocity.getPointer(),
		  vars->uVelocityCoeff[Arches::AE].getPointer(), 
		  vars->uVelocityCoeff[Arches::AW].getPointer(), 
		  vars->uVelocityCoeff[Arches::AN].getPointer(), 
		  vars->uVelocityCoeff[Arches::AS].getPointer(), 
		  vars->uVelocityCoeff[Arches::AT].getPointer(), 
		  vars->uVelocityCoeff[Arches::AB].getPointer(), 
		  vars->uVelocityCoeff[Arches::AP].getPointer(), 
		  vars->uVelNonlinearSrc.getPointer(),
		  e1.get_objs(), f1.get_objs(), e2.get_objs(), f2.get_objs(),
		  e3.get_objs(), f3.get_objs(), &theta);

      computeVelResidual(pc, patch, old_dw, new_dw, index, vars);
      velResid = vars->residUVel;
      ++velIter;
    } while((velIter < d_maxSweeps)&&((velResid > d_residual*nlResid)||
				      (velResid > trunc_conv)));
    cerr << "After u Velocity solve " << velIter << " " << velResid << endl;
    cerr << "After u Velocity solve " << nlResid << " " << trunc_conv <<  endl;
#else
    FORT_EXPLICIT(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  vars->uVelocity.getPointer(),
		  vars->old_uVelocity.getPointer(),
		  vars->uVelocityCoeff[Arches::AE].getPointer(), 
		  vars->uVelocityCoeff[Arches::AW].getPointer(), 
		  vars->uVelocityCoeff[Arches::AN].getPointer(), 
		  vars->uVelocityCoeff[Arches::AS].getPointer(), 
		  vars->uVelocityCoeff[Arches::AT].getPointer(), 
		  vars->uVelocityCoeff[Arches::AB].getPointer(), 
		  vars->uVelocityCoeff[Arches::AP].getPointer(), 
		  vars->uVelNonlinearSrc.getPointer(),
		  domLoDen.get_pointer(), domHiDen.get_pointer(),
		  vars->old_density.getPointer(), 
		  cellinfo->sewu.get_objs(), cellinfo->sns.get_objs(),
		  cellinfo->stb.get_objs(), &delta_t);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After U Vel Explicit solve : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "U Vel for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->uVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif

    vars->residUVel = 1.0E-7;
    vars->truncUVel = 1.0;
#endif
    break;
  case Arches::YDIR:
    domLo = vars->vVelocity.getFortLowIndex();
    domHi = vars->vVelocity.getFortHighIndex();
    idxLo = patch->getSFCYFORTLowIndex();
    idxHi = patch->getSFCYFORTHighIndex();

    Size = domHi - domLo + IntVector(1,1,1);

    e1.resize(Size.x());
    f1.resize(Size.x());
    e2.resize(Size.y());
    f2.resize(Size.y());
    e3.resize(Size.z());
    f3.resize(Size.z());

    old_dw->get(resid, lab->d_vVelResidPSLabel);
    old_dw->get(trunc, lab->d_vVelTruncPSLabel);

    nlResid = resid;
    trunc_conv = trunc*1.0E-7;
#if implicit_defined

    do {
      //fortran call for lineGS solver
      FORT_LINEGS(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  vars->vVelocity.getPointer(),
		  vars->vVelocityCoeff[Arches::AE].getPointer(), 
		  vars->vVelocityCoeff[Arches::AW].getPointer(), 
		  vars->vVelocityCoeff[Arches::AN].getPointer(), 
		  vars->vVelocityCoeff[Arches::AS].getPointer(), 
		  vars->vVelocityCoeff[Arches::AT].getPointer(), 
		  vars->vVelocityCoeff[Arches::AB].getPointer(), 
		  vars->vVelocityCoeff[Arches::AP].getPointer(), 
		  vars->vVelNonlinearSrc.getPointer(),
		  e1.get_objs(), f1.get_objs(), e2.get_objs(), f2.get_objs(),
		  e3.get_objs(), f3.get_objs(), &theta);

      computeVelResidual(pc, patch, old_dw, new_dw, index, vars);
      velResid = vars->residVVel;
      ++velIter;
    } while((velIter < d_maxSweeps)&&((velResid > d_residual*nlResid)||
				      (velResid > trunc_conv)));
    cerr << "After v Velocity solve " << velIter << " " << velResid << endl;
    cerr << "After v Velocity solve " << nlResid << " " << trunc_conv <<  endl;
#else
    FORT_EXPLICIT(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  vars->vVelocity.getPointer(),
		  vars->old_vVelocity.getPointer(),
		  vars->vVelocityCoeff[Arches::AE].getPointer(), 
		  vars->vVelocityCoeff[Arches::AW].getPointer(), 
		  vars->vVelocityCoeff[Arches::AN].getPointer(), 
		  vars->vVelocityCoeff[Arches::AS].getPointer(), 
		  vars->vVelocityCoeff[Arches::AT].getPointer(), 
		  vars->vVelocityCoeff[Arches::AB].getPointer(), 
		  vars->vVelocityCoeff[Arches::AP].getPointer(), 
		  vars->vVelNonlinearSrc.getPointer(),
		  domLoDen.get_pointer(), domHiDen.get_pointer(),
		  vars->old_density.getPointer(), 
		  cellinfo->sew.get_objs(), cellinfo->snsv.get_objs(),
		  cellinfo->stb.get_objs(), &delta_t);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After V Vel Explicit solve : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "V Vel for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->vVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif

    vars->residVVel = 1.0E-7;
    vars->truncVVel = 1.0;
#endif
    break;
  case Arches::ZDIR:
    domLo = vars->wVelocity.getFortLowIndex();
    domHi = vars->wVelocity.getFortHighIndex();
    idxLo = patch->getSFCYFORTLowIndex();
    idxHi = patch->getSFCYFORTHighIndex();

    Size = domHi - domLo + IntVector(1,1,1);

    e1.resize(Size.x());
    f1.resize(Size.x());
    e2.resize(Size.y());
    f2.resize(Size.y());
    e3.resize(Size.z());
    f3.resize(Size.z());

    old_dw->get(resid, lab->d_wVelResidPSLabel);
    old_dw->get(trunc, lab->d_wVelTruncPSLabel);

    nlResid = resid;
    trunc_conv = trunc*1.0E-7;
#if implicit_defined
    do {
      //fortran call for lineGS solver
      FORT_LINEGS(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  vars->wVelocity.getPointer(),
		  vars->wVelocityCoeff[Arches::AE].getPointer(), 
		  vars->wVelocityCoeff[Arches::AW].getPointer(), 
		  vars->wVelocityCoeff[Arches::AN].getPointer(), 
		  vars->wVelocityCoeff[Arches::AS].getPointer(), 
		  vars->wVelocityCoeff[Arches::AT].getPointer(), 
		  vars->wVelocityCoeff[Arches::AB].getPointer(), 
		  vars->wVelocityCoeff[Arches::AP].getPointer(), 
		  vars->wVelNonlinearSrc.getPointer(),
		  e1.get_objs(), f1.get_objs(), e2.get_objs(), f2.get_objs(),
		  e3.get_objs(), f3.get_objs(), &theta);

      computeVelResidual(pc, patch, old_dw, new_dw, index, vars);
      velResid = vars->residWVel;
      ++velIter;
    } while((velIter < d_maxSweeps)&&((velResid > d_residual*nlResid)||
				      (velResid > trunc_conv)));
    cerr << "After w Velocity solve " << velIter << " " << velResid << endl;
    cerr << "After w Velocity solve " << nlResid << " " << trunc_conv <<  endl;
#else
    FORT_EXPLICIT(domLo.get_pointer(), domHi.get_pointer(),
		  idxLo.get_pointer(), idxHi.get_pointer(),
		  vars->wVelocity.getPointer(),
		  vars->old_wVelocity.getPointer(),
		  vars->wVelocityCoeff[Arches::AE].getPointer(), 
		  vars->wVelocityCoeff[Arches::AW].getPointer(), 
		  vars->wVelocityCoeff[Arches::AN].getPointer(), 
		  vars->wVelocityCoeff[Arches::AS].getPointer(), 
		  vars->wVelocityCoeff[Arches::AT].getPointer(), 
		  vars->wVelocityCoeff[Arches::AB].getPointer(), 
		  vars->wVelocityCoeff[Arches::AP].getPointer(), 
		  vars->wVelNonlinearSrc.getPointer(),
		  domLoDen.get_pointer(), domHiDen.get_pointer(),
		  vars->old_density.getPointer(), 
		  cellinfo->sew.get_objs(), cellinfo->sns.get_objs(),
		  cellinfo->stbw.get_objs(), &delta_t);

#ifdef ARCHES_VEL_DEBUG
    cerr << " After W Vel Explicit solve : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "W Vel for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->wVelocity[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif

    vars->residWVel = 1.0E-7;
    vars->truncWVel = 1.0;
#endif
    break;
  default:
    throw InvalidValue("Invalid index in LinearSolver for velocity");
  }
}

//****************************************************************************
// Calculate Scalar residuals
//****************************************************************************
void 
PetscSolver::computeScalarResidual(const ProcessorGroup* ,
				  const Patch* patch,
				  DataWarehouseP& ,
				  DataWarehouseP& , 
				  int index,
				  ArchesVariables* vars)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo = vars->scalar.getFortLowIndex();
  IntVector domHi = vars->scalar.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  //fortran call

  FORT_COMPUTERESID(domLo.get_pointer(), domHi.get_pointer(),
		    idxLo.get_pointer(), idxHi.get_pointer(),
		    vars->scalar.getPointer(),
		    vars->residualScalar.getPointer(),
		    vars->scalarCoeff[Arches::AE].getPointer(), 
		    vars->scalarCoeff[Arches::AW].getPointer(), 
		    vars->scalarCoeff[Arches::AN].getPointer(), 
		    vars->scalarCoeff[Arches::AS].getPointer(), 
		    vars->scalarCoeff[Arches::AT].getPointer(), 
		    vars->scalarCoeff[Arches::AB].getPointer(), 
		    vars->scalarCoeff[Arches::AP].getPointer(), 
		    vars->scalarNonlinearSrc.getPointer(),
		    &vars->residScalar, &vars->truncScalar);
}


//****************************************************************************
// Actual calculation of order of magnitude term for Scalar equation
//****************************************************************************
void 
PetscSolver::computeScalarOrderOfMagnitude(const ProcessorGroup* ,
				const Patch* ,
				DataWarehouseP& ,
				DataWarehouseP& , ArchesVariables* )
{

  //&vars->truncScalar

}

//****************************************************************************
// Scalar Underrelaxation
//****************************************************************************
void 
PetscSolver::computeScalarUnderrelax(const ProcessorGroup* ,
				    const Patch* patch,
				    DataWarehouseP& ,
				    DataWarehouseP& , 
				    int index,
				    ArchesVariables* vars)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo = vars->scalar.getFortLowIndex();
  IntVector domHi = vars->scalar.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();

  //fortran call
  FORT_UNDERELAX(domLo.get_pointer(), domHi.get_pointer(),
		 idxLo.get_pointer(), idxHi.get_pointer(),
		 vars->scalar.getPointer(),
		 vars->scalarCoeff[Arches::AP].getPointer(), 
		 vars->scalarNonlinearSrc.getPointer(),
		 &d_underrelax);
}

//****************************************************************************
// Scalar Solve
//****************************************************************************
void 
PetscSolver::scalarLisolve(const ProcessorGroup* pc,
			  const Patch* patch,
			  DataWarehouseP& old_dw,
			  DataWarehouseP& new_dw, 
			  int index, double delta_t,
			  ArchesVariables* vars,
			  CellInformation* cellinfo,
			  const ArchesLabel* lab)
{
  // Get the patch bounds and the variable bounds
  IntVector domLo = vars->scalar.getFortLowIndex();
  IntVector domHi = vars->scalar.getFortHighIndex();
  IntVector idxLo = patch->getCellFORTLowIndex();
  IntVector idxHi = patch->getCellFORTHighIndex();
  // for explicit solver
  IntVector domLoDen = vars->old_density.getFortLowIndex();
  IntVector domHiDen = vars->old_density.getFortHighIndex();

  Array1<double> e1;
  Array1<double> f1;
  Array1<double> e2;
  Array1<double> f2;
  Array1<double> e3;
  Array1<double> f3;

  IntVector Size = domHi - domLo + IntVector(1,1,1);

  e1.resize(Size.x());
  f1.resize(Size.x());
  e2.resize(Size.y());
  f2.resize(Size.y());
  e3.resize(Size.z());
  f3.resize(Size.z());

  sum_vartype resid;
  sum_vartype trunc;

  old_dw->get(resid, lab->d_scalarResidLabel);
  old_dw->get(trunc, lab->d_scalarTruncLabel);

  double nlResid = resid;
  double trunc_conv = trunc*1.0E-7;
  double theta = 0.5;
  int scalarIter = 0;
  double scalarResid = 0.0;
#if implict_defined
  do {
    //fortran call for lineGS solver
    FORT_LINEGS(domLo.get_pointer(), domHi.get_pointer(),
		idxLo.get_pointer(), idxHi.get_pointer(),
		vars->scalar.getPointer(),
		vars->scalarCoeff[Arches::AE].getPointer(), 
		vars->scalarCoeff[Arches::AW].getPointer(), 
		vars->scalarCoeff[Arches::AN].getPointer(), 
		vars->scalarCoeff[Arches::AS].getPointer(), 
		vars->scalarCoeff[Arches::AT].getPointer(), 
		vars->scalarCoeff[Arches::AB].getPointer(), 
		vars->scalarCoeff[Arches::AP].getPointer(), 
		vars->scalarNonlinearSrc.getPointer(),
		e1.get_objs(), f1.get_objs(), e2.get_objs(), f2.get_objs(),
		e3.get_objs(), f3.get_objs(), &theta);
    computeScalarResidual(pc, patch, old_dw, new_dw, index, vars);
    scalarResid = vars->residScalar;
    ++scalarIter;
  } while((scalarIter < d_maxSweeps)&&((scalarResid > d_residual*nlResid)||
				      (scalarResid > trunc_conv)));
  cerr << "After scalar " << index <<" solve " << scalarIter << " " << scalarResid << endl;
  cerr << "After scalar " << index <<" solve " << nlResid << " " << trunc_conv <<  endl;
#endif
     FORT_EXPLICIT(domLo.get_pointer(), domHi.get_pointer(),
		   idxLo.get_pointer(), idxHi.get_pointer(),
		   vars->scalar.getPointer(), vars->old_scalar.getPointer(),
		   vars->scalarCoeff[Arches::AE].getPointer(), 
		   vars->scalarCoeff[Arches::AW].getPointer(), 
		   vars->scalarCoeff[Arches::AN].getPointer(), 
		   vars->scalarCoeff[Arches::AS].getPointer(), 
		   vars->scalarCoeff[Arches::AT].getPointer(), 
		   vars->scalarCoeff[Arches::AB].getPointer(), 
		   vars->scalarCoeff[Arches::AP].getPointer(), 
		   vars->scalarNonlinearSrc.getPointer(),
		   domLoDen.get_pointer(), domHiDen.get_pointer(),
		   vars->old_density.getPointer(), 
		   cellinfo->sew.get_objs(), cellinfo->sns.get_objs(),
		   cellinfo->stb.get_objs(), &delta_t);
#ifdef ARCHES_VEL_DEBUG
    cerr << " After Scalar Explicit solve : " << endl;
    for (int ii = domLo.x(); ii <= domHi.x(); ii++) {
      cerr << "Scalar for ii = " << ii << endl;
      for (int jj = domLo.y(); jj <= domHi.y(); jj++) {
	for (int kk = domLo.z(); kk <= domHi.z(); kk++) {
	  cerr.width(14);
	  cerr << vars->scalar[IntVector(ii,jj,kk)] << " " ; 
	}
	cerr << endl;
      }
    }
#endif


    vars->residScalar = 1.0E-7;
    vars->truncScalar = 1.0;
   
}

//
// $Log$
// Revision 1.5  2000/09/12 22:34:02  sparker
// Moved petsc include to .cc file
//
// Revision 1.4  2000/09/12 18:12:12  sparker
// Fixed bugs - petsc solver now works in serial
//
// Revision 1.3  2000/09/12 15:45:57  sparker
// Changes to petsc solver almost work
//
// Revision 1.2  2000/09/11 16:41:59  rawat
// modified petsc solver
//
// Revision 1.1  2000/09/07 23:07:17  rawat
// fixed some bugs in bc and added pressure solver using petsc
//
// Revision 1.21  2000/08/23 06:20:52  bbanerje
// 1) Results now correct for pressure solve.
// 2) Modified BCU, BCV, BCW to add stuff for pressure BC.
// 3) Removed some bugs in BCU, V, W.
// 4) Coefficients for MOM Solve not computed correctly yet.
//
// Revision 1.20  2000/08/17 20:32:00  rawat
// Fixed some bugs
//
// Revision 1.19  2000/08/15 00:23:32  rawat
// added explicit solve for momentum and scalar eqns
//
// Revision 1.18  2000/08/12 23:53:19  bbanerje
// Added Linegs part to the solver.
//
// Revision 1.17  2000/08/11 21:26:36  rawat
// added linear solver for pressure eqn
//
// Revision 1.16  2000/08/01 23:28:43  skumar
// Added residual calculation procedure and modified templates in linear
// solver.  Added template for order-of-magnitude term calculation.
//
// Revision 1.15  2000/08/01 06:18:38  bbanerje
// Made ScalarSolver similar to PressureSolver and MomentumSolver.
//
// Revision 1.14  2000/07/28 02:31:00  rawat
// moved all the labels in ArchesLabel. fixed some bugs and added matrix_dw to store matrix
// coeffecients
//
// Revision 1.13  2000/07/08 23:42:55  bbanerje
// Moved all enums to Arches.h and made corresponding changes.
//
// Revision 1.12  2000/07/08 08:03:34  bbanerje
// Readjusted the labels upto uvelcoef, removed bugs in CellInformation,
// made needed changes to uvelcoef.  Changed from StencilMatrix::AE etc
// to Arches::AE .. doesn't like enums in templates apparently.
//
// Revision 1.11  2000/06/29 22:56:43  bbanerje
// Changed FCVars to SFC[X,Y,Z]Vars, and added the neceesary getIndex calls.
//
// Revision 1.10  2000/06/22 23:06:37  bbanerje
// Changed velocity related variables to FCVariable type.
// ** NOTE ** We may need 3 types of FCVariables (one for each direction)
//
// Revision 1.9  2000/06/21 07:51:01  bbanerje
// Corrected new_dw, old_dw problems, commented out intermediate dw (for now)
// and made the stuff go through schedule_time_advance.
//
// Revision 1.8  2000/06/18 01:20:16  bbanerje
// Changed names of varlabels in source to reflect the sequence of tasks.
// Result : Seg Violation in addTask in MomentumSolver
//
// Revision 1.7  2000/06/17 07:06:26  sparker
// Changed ProcessorContext to ProcessorGroup
//
// Revision 1.6  2000/06/12 21:29:59  bbanerje
// Added first Fortran routines, added Stencil Matrix where needed,
// removed unnecessary CCVariables (e.g., sources etc.)
//
// Revision 1.5  2000/06/07 06:13:56  bbanerje
// Changed CCVariable<Vector> to CCVariable<double> for most cases.
// Some of these variables may not be 3D Vectors .. they may be Stencils
// or more than 3D arrays. Need help here.
//
// Revision 1.4  2000/06/04 22:40:15  bbanerje
// Added Cocoon stuff, changed task, require, compute, get, put arguments
// to reflect new declarations. Changed sub.mk to include all the new files.
//
//

