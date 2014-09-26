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

#include <CCA/Components/MPM/ReactiveFlow/ConcentrationContact/ConcentrationContactFactory.h>
#include <CCA/Components/MPM/ReactiveFlow/ConcentrationContact/STConcentrationContact.h>
#include <CCA/Components/MPM/ReactiveFlow/ConcentrationContact/NullConcentrationContact.h>
#include <Core/Malloc/Allocator.h>
#include <string>
using std::cerr;

using namespace Uintah;

ConcentrationContact* ConcentrationContactFactory::create(const ProblemSpecP& ps,
                                              SimulationStateP& d_sS, 
                                              MPMLabel* lb,MPMFlags* flag)
{
   ProblemSpecP mpm_ps = 
     ps->findBlockWithOutAttribute("MaterialProperties")->findBlock("MPM");

   for( ProblemSpecP child = mpm_ps->findBlock("concentration_contact"); child != 0;
                child = child->findNextBlock("concentration_contact")) {
     return( scinew STConcentrationContact(child,d_sS,lb,flag) );
   }

   ProblemSpecP child; 
   return( scinew NullConcentrationContact(child,d_sS,lb,flag) );
}
