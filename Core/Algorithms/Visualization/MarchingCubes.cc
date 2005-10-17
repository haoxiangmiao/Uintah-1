/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
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

//    File   : MarchingCubes.cc
//    Author : Martin Cole
//    Date   : Fri Jun 15 16:19:22 2001

#include <Core/Algorithms/Visualization/MarchingCubes.h>
#include <Core/Algorithms/Visualization/HexMC.h>
#include <Core/Algorithms/Visualization/UHexMC.h>
#include <Core/Algorithms/Visualization/PrismMC.h>
#include <Core/Algorithms/Visualization/TetMC.h>
#include <Core/Algorithms/Visualization/TriMC.h>
#include <Core/Algorithms/Visualization/QuadMC.h>
#include <Core/Algorithms/Visualization/EdgeMC.h>

namespace SCIRun {

using namespace std;

MarchingCubesAlg::MarchingCubesAlg() : 
  np_(1) 
{}

MarchingCubesAlg::~MarchingCubesAlg() 
{}

const string& 
MarchingCubesAlg::get_h_file_path() {
  static const string path(TypeDescription::cc_to_h(__FILE__));
  return path;
}

CompileInfoHandle
MarchingCubesAlg::get_compile_info(const TypeDescription *td) {

  string subname;
  string subinc;
  string sname = td->get_name("", "");
  string out_mesh_inc("");
  string out_basis_inc("");
  string out_mesh_inc2("");
  string out_basis_inc2("");
  
  //Test for LatVolField inheritance...
  if (sname.find("LatVolMesh") != string::npos ||
      sname.find("StructHexVolMesh") != string::npos) {
    // we are dealing with a lattice vol or inherited version
    subname.append("HexMC<" + td->get_name() + "> ");
    subinc.append(HexMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/QuadSurfMesh.h";
    out_basis_inc = "../src/Core/Basis/QuadBilinearLgn.h";
    out_mesh_inc2 = "../src/Core/Datatypes/TriSurfMesh.h";
    out_basis_inc2 = "../src/Core/Basis/TriLinearLgn.h";
  } else if (sname.find("TetVolMesh") != string::npos) {
    subname.append("TetMC<" + td->get_name() + "> ");
    subinc.append(TetMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/TriSurfMesh.h";
    out_basis_inc = "../src/Core/Basis/TriLinearLgn.h";
  } else if (sname.find("PrismVolMesh") != string::npos) {
    subname.append("PrismMC<" + td->get_name() + "> ");
    subinc.append(PrismMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/TriSurfMesh.h";
    out_basis_inc = "../src/Core/Basis/TriLinearLgn.h";
  } else if (sname.find("HexVolMesh") != string::npos) {
    subname.append("UHexMC<" + td->get_name() + "> ");
    subinc.append(UHexMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/QuadSurfMesh.h";
    out_basis_inc = "../src/Core/Basis/QuadBilinearLgn.h";
    out_mesh_inc2 = "../src/Core/Datatypes/TriSurfMesh.h";
    out_basis_inc2 = "../src/Core/Basis/TriLinearLgn.h";
  } else if (sname.find("TriSurf") != string::npos) {
    subname.append("TriMC<" + td->get_name() + "> ");
    subinc.append(TriMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/CurveMesh.h";
    out_basis_inc = "../src/Core/Basis/CrvLinearLgn.h";
  } else if (sname.find("QuadSurfMesh") != string::npos ||
	     sname.find("ImageMesh") != string::npos) {
    subname.append("QuadMC<" + td->get_name() + "> ");
    subinc.append(QuadMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/CurveMesh.h";
    out_basis_inc = "../src/Core/Basis/CrvLinearLgn.h";
  } else if (sname.find("CurveMesh") != string::npos) {
    subname.append("EdgeMC<" + td->get_name() + "> ");
    subinc.append(EdgeMCBase::get_h_file_path());
    out_mesh_inc = "../src/Core/Datatypes/PointCloudMesh.h";
    out_basis_inc = "../src/Core/Basis/Constant.h";
  } else {
    cerr << "Unsupported field type." << endl;
    subname.append("Cannot compile this unsupported type");
  }

  string fname("MarchingCubes." + td->get_filename() + ".");
  CompileInfo *rval = scinew CompileInfo(fname, "MarchingCubesAlg", 
					 "MarchingCubes", subname);
  rval->add_include(get_h_file_path());
  rval->add_include(subinc);
  // likely needed as these are the output field types
  rval->add_mesh_include(out_mesh_inc);
  if (out_mesh_inc2 != "") 
    rval->add_mesh_include(out_mesh_inc2);
  rval->add_basis_include(out_basis_inc);
  if (out_basis_inc2 != "")
    rval->add_basis_include(out_basis_inc2);
  td->fill_compile_info(rval);
  return rval;
}

}
