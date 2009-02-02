/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/


/*****************************************************************************
 *
 * Copyright (c) 2000 - 2008, Lawrence Livermore National Security, LLC
 * Produced at the Lawrence Livermore National Laboratory
 * LLNL-CODE-400142
 * All rights reserved.
 *
 * This file is  part of VisIt. For  details, see https://visit.llnl.gov/.  The
 * full copyright notice is contained in the file COPYRIGHT located at the root
 * of the VisIt distribution or at http://www.llnl.gov/visit/copyright.html.
 *
 * Redistribution  and  use  in  source  and  binary  forms,  with  or  without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of  source code must  retain the above  copyright notice,
 *    this list of conditions and the disclaimer below.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
 *    documentation and/or other materials provided with the distribution.
 *  - Neither the name of  the LLNS/LLNL nor the names of  its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
 * ARE  DISCLAIMED. IN  NO EVENT  SHALL LAWRENCE  LIVERMORE NATIONAL  SECURITY,
 * LLC, THE  U.S.  DEPARTMENT OF  ENERGY  OR  CONTRIBUTORS BE  LIABLE  FOR  ANY
 * DIRECT,  INDIRECT,   INCIDENTAL,   SPECIAL,   EXEMPLARY,  OR   CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
 * CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
 * LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
 * OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 *****************************************************************************/

// ************************************************************************* //
//                            avtudaReaderMTMDFileFormat.h                   //
// ************************************************************************* //

#ifndef AVT_udaReaderMTMD_FILE_FORMAT_H
#define AVT_udaReaderMTMD_FILE_FORMAT_H

#include <Packages/Uintah/StandAlone/tools/uda2vis/particleData.h>

#include <avtMTMDFileFormat.h>

#include <string>

// ****************************************************************************
//  Class: avtudaReaderMTMDFileFormat
//
//  Purpose:
//      Reads in udaReaderMTMD files as a plugin to VisIt.
//
//  Programmer: sshankar -- generated by xml2avt
//  Creation:   Tue May 13 19:02:26 PST 2008
//
// ****************************************************************************

class avtudaReaderMTMDFileFormat : public avtMTMDFileFormat
{
public:
  avtudaReaderMTMDFileFormat(const char *);
  virtual           ~avtudaReaderMTMDFileFormat();

  //
  // This is used to return unconvention data -- ranging from material
  // information to information about block connectivity.
  //
  // virtual void      *GetAuxiliaryData(const char *var, int timestep, 
  //                                     int domain, const char *type, void *args, 
  //                                     DestructorFunction &);
  //

  //
  // If you know the times and cycle numbers, overload this function.
  // Otherwise, VisIt will make up some reasonable ones for you.
  //
  // virtual void        GetCycles(std::vector<int> &);
  // virtual void        GetTimes(std::vector<double> &);
  //

  virtual int           GetNTimesteps(void);

  virtual const char    *GetType(void)   { return "udaReaderMTMD"; };
  virtual void           FreeUpResources(void);
  // virtual void          ActivateTimestep(int); 

  virtual vtkDataSet    *GetMesh(int, int, const char *);
  virtual vtkDataArray  *GetVar(int, int, const char *);
  virtual vtkDataArray  *GetVectorVar(int, int, const char *);

protected:
  // DATA MEMBERS
        
  int timeSteps, currTimeStep, lastTimeStep;
  std::string  folder;
        
  void  * libHandle;
  char  * error, arr2d[128][128];
        
  levelPatchVec * levelPatchVecPtr;
  patchInfoVec  * patchInfoVecPtr;
        
  double ***refMatrix; // scalars
  vecVal ***vecValMatrix;
  tenVal ***tenValMatrix;
        
  int         currLevel;
  std::string currVar, currMesh;
  // bool callDomainNesting;
        
  int ncomps;
        
  timeStep*        (*processData)(int, char[][128], int, bool, int, bool, int);
  udaVars*         (*getVarList)(const std::string&);
  int*             (*getTimeSteps)(const std::string&);
  double*          (*getBBox)(const std::string&, int, int);
  double*          (*getPatchBBox)(const std::string&, int, int, int);
  int*             (*getPatchIndex)(const std::string&, int, int, int, const std::string&);
  varMatls*        (*getMaterials)(const std::string&, const std::string&, int);
  levelPatchVec*   (*getTotalNumPatches)(const std::string&, int);
  patchInfoVec*    (*getPatchInfo)(const std::string&, int, const std::string&, bool);
  int*             (*getNumPatches)(const std::string&, int, int);
  int*             (*getNumLevels)(const std::string&, int);

  virtual void     PopulateDatabaseMetaData(avtDatabaseMetaData *, int);
  // virtual void     RegisterVariableList(const char *,
  // const vector<CharStrRef> &);

  void             GetLevelAndLocalPatchNumber(int, int, int&, int&);
  void             CalculateDomainNesting(int);
        
  virtual bool     HasInvariantMetaData(void) const { return false; };
  virtual bool     HasInvariantSIL(void) const { return false; };
};

#endif
