#include "FieldSelection.h"
#include "Packages/Uintah/Core/Exceptions/ProblemSetupException.h"
#include "utils.h"
#include <algorithm>
#include <iostream>

namespace SCIRun {
  using namespace std;
  using namespace Uintah;
  
  FieldSelection::FieldSelection(Args & args, const vector<string> & allfields)
  {
    // let user select fields using 
    // -fields f1,f2,f3
    string fieldnames = args.getString("field", "all");
    cout << "fieldnames: " << fieldnames << endl;
    
    if(fieldnames=="all") {
      this->fieldnames = allfields;      
    } else {
      vector<string> requested_fields = split(fieldnames,',',false);
      for(vector<string>::const_iterator fit(requested_fields.begin());
          fit!=requested_fields.end();fit++) {
        if(!count(allfields.begin(), allfields.end(), *fit))
          throw ProblemSetupException("Failed to find field called "+*fit,__FILE__,__LINE__);
        this->fieldnames.push_back(*fit);
      }
    }
    
    // let user select components using 
    // -diagnostic name1,name2,name3
    //
    string diagnames = args.getString("diagnostic", "value");
    if(diagnames!="all") {
      this->diagnames = split(diagnames, ',', false);
      for(vector<string>::const_iterator dit(this->diagnames.begin());
          dit!=this->diagnames.end();dit++)
        cout << "   writing " << *dit << endl;
    }
    
    // let user select tensor operations
    // -tensor_op op1,op2
    //
    string opnames = args.getString("tensor_op", "identity");
    this->opnames = split(opnames, ',', false);
    
    // let user specify materials using
    // -material mat1,mat2
    string matnames = args.getString("material", "all");
    if(matnames!="all") {
      vector<string> matlist = split(matnames, ',', false);
      for(vector<string>::const_iterator mit(matlist.begin());mit!=matlist.end();mit++) {
        int imat = (int)strtol(mit->c_str(), 0, 10);
        this->mats.push_back(imat);
      }
    }
  }
  
  bool
  FieldSelection::wantField(string fieldname) const
  {
    return (count(this->fieldnames.begin(), this->fieldnames.end(), fieldname)>0);
  }
  
  bool
  FieldSelection::wantMaterial(int imat) const 
  {
    if(this->mats.size()==0)
      return true;
    else
      return (count(mats.begin(), mats.end(), imat)>0);
  }
  
  bool
  FieldSelection::wantDiagnostic(string diagname) const 
  {
    if(this->diagnames.size()==0) {
      return true;
    } else {
      return (count(diagnames.begin(), diagnames.end(), diagname)>0);
    }
  }
  
  bool
  FieldSelection::wantTensorOp(string diagname) const 
  {
    return (count(opnames.begin(), opnames.end(), diagname)>0);
  }
}
