
/*
 *  BaseConstraint.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Aug. 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#include <stdio.h>
#include <string.h>
#include <Constraints/BaseConstraint.h>


BaseConstraint::BaseConstraint( const clString& name, const Index nschemes,
				const Index varCount )
: name(name), nschemes(nschemes), varCount(varCount),
  vars(varCount), var_indexs(varCount), var_choices(nschemes, varCount)
{
   whichMethod = 0;
}


BaseConstraint::~BaseConstraint()
{
}


void
BaseConstraint::Priorities( const VPriority p1,
			    const VPriority p2,
			    const VPriority p3,
			    const VPriority p4,
			    const VPriority p5,
			    const VPriority p6,
			    const VPriority p7 )
{
   Index p=0;
   
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p1);
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p2);
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p3);
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p4);
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p5);
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p6);
   if (p == varCount) return;
   vars[p]->RegisterPriority(var_indexs[p++], p7);
}


void
BaseConstraint::VarChoices( const Scheme scheme,
			    const Index i1,
			    const Index i2,
			    const Index i3,
			    const Index i4,
			    const Index i5,
			    const Index i6,
			    const Index i7 )
{
   ASSERT(scheme<nschemes);
   Index p=0;
   
   if (p == varCount) return;
   var_choices(scheme, p++) = i1;
   if (p == varCount) return;
   var_choices(scheme, p++) = i2;
   if (p == varCount) return;
   var_choices(scheme, p++) = i3;
   if (p == varCount) return;
   var_choices(scheme, p++) = i4;
   if (p == varCount) return;
   var_choices(scheme, p++) = i5;
   if (p == varCount) return;
   var_choices(scheme, p++) = i6;
   if (p == varCount) return;
   var_choices(scheme, p++) = i7;
}


int
BaseConstraint::Satisfy( const Index, const Scheme, const Real, BaseVariable*&, VarCore& )
{
   ASSERT(!"BaseConstraint: Can't satisfy!");
   return 0;
}


void
BaseConstraint::print( ostream& os )
{
   int i;

   for (Index j=0; j < nschemes; j++) {
      os << name << " (" << SchemeString((Scheme)j) << ") (";
      for (i = 0; i < varCount; i++) {
	 if (i != whichMethod) {
	    os << "\t";
	    vars[i]->printc(os, var_indexs[i]);
	    os << " (->" << var_choices(j, i) << ")";
	    os << endl;
	 }
      }
      os << "\t-> ";
      if (whichMethod < varCount) {
	 vars[whichMethod]->printc(os, var_indexs[whichMethod]);
	 os << " (->" << var_choices(j, whichMethod) << ")";
      } else {
	 os << "(Special option.";
      }
      os << ")" << endl;
   }
}


void
BaseConstraint::printc( ostream& os, const Scheme scheme )
{
   int i;

   os << name << " (" << SchemeString(scheme) << ") (";
   os << "Called by " << callingMethod << ") (" << endl;
   for (i = 0; i < varCount; i++) {
      if (i != whichMethod) {
	 os << "\t";
	 vars[i]->printc(os, var_indexs[i]);
	 os << " (->" << var_choices(scheme, i) << ")";
	 os << endl;
      }
   }
   os << "\t-> ";
   vars[whichMethod]->printc(os, var_indexs[whichMethod]);
   os << " (->" << var_choices(scheme, whichMethod) << ")";
   os << ")" << endl;
}


void
BaseConstraint::Register()
{
   Index index;

   for (index = 0; index < varCount; index++)
      var_indexs[index] = vars[index]->Register(this, index);
}


