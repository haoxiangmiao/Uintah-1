#include <CCA/Components/ICE/EOS/EquationOfStateFactory.h>
#include <CCA/Components/ICE/EOS/IdealGas.h>
#include <CCA/Components/ICE/EOS/JWL.h>
#include <CCA/Components/ICE/EOS/TST.h>
#include <CCA/Components/ICE/EOS/JWLC.h>
#include <CCA/Components/ICE/EOS/Murnahan.h>
#include <CCA/Components/ICE/EOS/Gruneisen.h>
#include <CCA/Components/ICE/EOS/Tillotson.h>
#include <Core/ProblemSpec/ProblemSpec.h>
#include <Core/Exceptions/ProblemSetupException.h>
#include <SCIRun/Core/Malloc/Allocator.h>
#include <fstream>
#include <iostream>
#include <string>

using std::cerr;
using std::ifstream;
using std::ofstream;

using namespace Uintah;

EquationOfState* EquationOfStateFactory::create(ProblemSpecP& ps)
{
    ProblemSpecP child = ps->findBlock("EOS");
    if(!child)
      throw ProblemSetupException("Cannot find EOS tag", __FILE__, __LINE__);
    std::string mat_type;
    if(!child->getAttribute("type",mat_type))
      throw ProblemSetupException("No type for EOS", __FILE__, __LINE__); 
    
    if (mat_type == "ideal_gas") 
      return(scinew IdealGas(child));
    else if (mat_type == "TST") 
      return(scinew TST(child));
    else if (mat_type == "JWL") 
      return(scinew JWL(child));
    else if (mat_type == "JWLC") 
      return(scinew JWLC(child));
    else if (mat_type == "Murnahan") 
      return(scinew Murnahan(child));
    else if (mat_type == "Gruneisen") 
      return(scinew Gruneisen(child));
    else if (mat_type == "Tillotson") 
      return(scinew Tillotson(child));    
    else
      throw ProblemSetupException("Unknown EOS Type R ("+mat_type+")", __FILE__, __LINE__);

}
