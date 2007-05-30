#include <CCA/Components/Solvers/HypreSolvers/HypreSolverBase.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverSMG.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverPFMG.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverSparseMSG.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverCG.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverHybrid.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverGMRES.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverAMG.h>
#include <CCA/Components/Solvers/HypreSolvers/HypreSolverFAC.h>
#include <CCA/Components/Solvers/HypreDriverStruct.h>
#include <CCA/Components/Solvers/HypreDriverSStruct.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <SCIRun/Core/Util/DebugStream.h>

#include <sci_comp_warn_fixes.h>

using namespace Uintah;
//__________________________________
//  To turn on normal output
//  setenv SCI_DEBUG "HYPRE_DOING_COUT:+"

static DebugStream cout_doing("HYPRE_DOING_COUT", false);
static DebugStream cout_dbg("HYPRE_DBG", false);

HypreSolverBase::HypreSolverBase(HypreDriver* driver,
                                 HyprePrecondBase* precond,
                                 const Priorities& priority) :
  _driver(driver), _precond(precond), _priority(priority), _requiresPar(true)
{
  cout_doing << Parallel::getMPIRank()<<" HypreSolverBase::constructor BEGIN" << "\n";
  assertInterface();

  // Initialize results section
  _results.numIterations = 0;
  _results.finalResNorm  = 1.23456e+30; // Large number
  cout_doing << Parallel::getMPIRank()<<"HypreSolverBase::constructor END" << "\n";
}
  
HypreSolverBase::~HypreSolverBase(void)
{
}
//______________________________________________________________________  
void
HypreSolverBase::assertInterface(void)
{ 
  cout_doing << Parallel::getMPIRank()<<" HypreSolverBase::assertInterface() BEGIN" << "\n";
  if (_priority.size() < 1) {
    throw InternalError("Solver created without interface priorities",
                        __FILE__, __LINE__);
 
  }

  // Intersect solver and preconditioner priorities
  if (_precond) {
    cout_dbg << "Intersect solver, precond priorities begin" << "\n";
    Priorities newSolverPriority;
    const Priorities& precondPriority = _precond->getPriority();
    for (unsigned int i = 0; i < _priority.size(); i++) {
      cout_dbg << "i = " << i << "\n";
      bool remove = false;
      for (unsigned int j = 0; j < _priority.size(); j++) {
        cout_dbg << "j = " << j << "\n";
        if (_priority[i] == precondPriority[j]) {
          // Remove this solver interface entry
          remove = true;
          break;
        }
        if (!remove) {
          newSolverPriority.push_back(_priority[i]);
        }
      }
    }
    _priority = newSolverPriority;
  } // end if (_precond)

  // Check whether solver requires ParCSR or not, because we need to
  // know about that in HypreDriver::makeLinearSystem. Also check the
  // correctness of the values of _priority.
  cout_dbg << "Check if solver requires par" << "\n";
  for (unsigned int i = 0; i < _priority.size(); i++) {
    if (_priority[i] == HypreInterfaceNA) {
      throw InternalError("Bad Solver interface priority "+_priority[i],
                          __FILE__, __LINE__);
    } else if ((_priority[i] != HypreParCSR) && (_requiresPar)) {
      // Modify this rule if we use other Hypre interfaces in the future.
      // See HypreTypes.h.
      _requiresPar = false;
    }
  }
  cout_dbg << "requiresPar = " << _requiresPar << "\n";

  cout_dbg << "Look for requested interface in solver priorities" << "\n";
  const HypreInterface& interface = _driver->getInterface();
  cout_dbg << "interface = " << interface << "\n";
  bool found = false;
  cout_dbg << "Solver priorities:" << "\n";
  for (unsigned int i = 0; i < _priority.size(); i++) {
    cout_dbg << "_priority[" << i << "] = " << _priority[i] << "\n";
    if (interface == _priority[i]) {
      // Found interface that solver can work with
      found = true;
      break;
    }
  }
  cout_dbg << "1. found = " << found << "\n";

  // See whether we can convert the Hypre data to a format we can
  // work with.
  if (!found) {
    cout_dbg << "Looking for possible conversions" << "\n";
    for (unsigned int i = 0; i < _priority.size(); i++) {
      cout_dbg << "i = " << i << "\n";
      // Try to convert from the current driver to _priority[i]
      if (_driver->isConvertable(_priority[i])) {
        // Conversion exists
        found = true;
        break;
      }
    }
  }

  cout_dbg << "2. found = " << found << "\n";

  if (!found) {
    ostringstream msg;
    msg << "Solver does not support Hypre interface " << interface;
    throw InternalError(msg.str(),__FILE__, __LINE__); 
  }
  cout_doing << Parallel::getMPIRank()<<" HypreSolverBase::assertInterface() END" << "\n";
}
//______________________________________________________________________
namespace Uintah {

  HypreSolverBase*
  newHypreSolver(const SolverType& solverType,
                 HypreDriver* driver,
                 HyprePrecondBase* precond)
    // Create a new solver object of specific solverType solver type
    // but a generic solver pointer type.
    // Include all derived solver classes here.
  {
    cout_doing << Parallel::getMPIRank()<<" HypreSolverBase::newHypreSolver() BEGIN" << "\n";
    const Priorities precondPriority;
    switch (solverType) {
    case SMG:
      {
        return scinew HypreSolverSMG(driver,precond);
      }
    case PFMG:
      {
        return scinew HypreSolverPFMG(driver,precond);
      }
    case SparseMSG:
      {
        return scinew HypreSolverSparseMSG(driver,precond);
      }
    case CG:
      {
        cout_dbg << "Doing new HypreSolverCG" << "\n";
        return scinew HypreSolverCG(driver,precond);
      }
    case Hybrid: 
      {
        return scinew HypreSolverHybrid(driver,precond);
      }
    case GMRES:
      {
        return scinew HypreSolverGMRES(driver,precond);
      }
    case AMG:
      {
        return scinew HypreSolverAMG(driver,precond);
      }
    case FAC:
      {
        return scinew HypreSolverFAC(driver,precond);
      }
    default:
      throw InternalError("Unsupported solver type: "+solverType,
                          __FILE__, __LINE__);
    } // switch (solverType)
    cout_doing << "newHypreSolver() END (shouldn't be reached)" << "\n";
    RETURN_0;
  }
//______________________________________________________________________
  SolverType
  getSolverType(const string& solverTitle)
  {
    // Determine solver type from title
    if ((solverTitle == "SMG") ||
        (solverTitle == "smg")) {
      return SMG;
    } else if ((solverTitle == "PFMG") ||
               (solverTitle == "pfmg")) {
      return PFMG;
    } else if ((solverTitle == "SparseMSG") ||
               (solverTitle == "sparsemsg")) {
      return SparseMSG;
    } else if ((solverTitle == "CG") ||
               (solverTitle == "cg") ||
               (solverTitle == "PCG") ||
               (solverTitle == "conjugategradient")) {
      return CG;
    } else if ((solverTitle == "Hybrid") ||
               (solverTitle == "hybrid")) {
      return Hybrid;
    } else if ((solverTitle == "GMRES") ||
               (solverTitle == "gmres")) {
      return GMRES;
    } else if ((solverTitle == "AMG") ||
               (solverTitle == "amg") ||
               (solverTitle == "BoomerAMG") ||
               (solverTitle == "boomeramg")) {
      return AMG;
    } else if ((solverTitle == "FAC") ||
               (solverTitle == "fac")) {
      return FAC;
    } else {
      throw InternalError("Unknown solver type: "+solverTitle,
                          __FILE__, __LINE__);
    } // end "switch" (solverTitle)
  } // end solverFromTitle()

  ostream&
  operator << (ostream& os, const SolverType& solverType)
    // Write a solver type (enum) to the stream os.
  {
    switch (solverType) {
    case SMG:       { os << "SMG"; break; }
    case PFMG:      { os << "PFMG"; break; }
    case SparseMSG: { os << "SparseMSG"; break; }
    case CG:        { os << "CG";  break; }
    case Hybrid:    { os << "Hybrid"; break; }
    case GMRES:     { os << "GMRES"; break; }
    case AMG:       { os << "AMG"; break; }
    case FAC:       { os << "FAC"; break; }
    default:        { os << "???"; break; }
    } // switch (solverType)

    return os;
  }

} // end namespace Uintah
