/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/


/*
 *  FieldMeasures.h:  Build a matrix of measured quantities (rows) 
 *                      associated with mesh simplices (columns)
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   December 2002
 *
 *  Copyright (C) 2002 SCI Group
 */

#if !defined(FieldMeasures_h)
#define FieldMeasures_h
  
#include <Core/Util/TypeDescription.h>
#include <Core/Util/DynamicLoader.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <Core/Datatypes/ColumnMatrix.h>
#include <Core/Geometry/Point.h>
#include <Core/Datatypes/Mesh.h>

namespace SCIRun {


class FieldMeasuresAlgo : public DynamicAlgoBase
{
public:
  virtual Matrix *execute(MeshHandle meshH, bool x, bool y, bool z,
		         bool idx, bool size, bool nnbrs, bool nbr_enum)=0;

  //! Support the dynamically compiled algorithm concept.
  static CompileInfoHandle get_compile_info(const TypeDescription *mesh_td,
					    const TypeDescription *simplex_td,
					    bool nnormals, bool fnormals);
};

template <class MESH, class SIMPLEX>
class FieldMeasuresAlgoT : public FieldMeasuresAlgo
{
public:
  //! virtual interface. 
  virtual Matrix *execute(MeshHandle meshH, bool x, bool y, bool z, bool idx,
			  bool nnbrs, bool size, bool nbr_enum);
};

//! MESH -- e.g. TetVolMeshHandle
//! SIMPLEX -- e.g. TetVolMesh::Node

// We have to pass in nbr_enum, in order to call synchronize neighbor info...
//   maybe there's a better way?

template <class MESH, class SIMPLEX>
Matrix *
FieldMeasuresAlgoT<MESH,SIMPLEX>::execute(MeshHandle meshH, bool x, bool y, 
					  bool z, bool idx, bool size,
					  bool nnbrs, bool nbr_enum)
{
  MESH *mesh = dynamic_cast<MESH *>(meshH.get_rep());
  int ncols=0;

  if (x)     ncols++;
  if (y)     ncols++;
  if (z)     ncols++;
  if (idx)   ncols++;
  if (nnbrs) ncols++;
  if (size)  ncols++;

  if (ncols==0) {
    cerr << "Error -- no measures selected.\n";
    return 0;
  }

  typename SIMPLEX::size_type nsimplices;
  mesh->size(nsimplices);
  Matrix *m;
  if (ncols==1) m = scinew ColumnMatrix(nsimplices);
  else m = scinew DenseMatrix(nsimplices, ncols);

  if (nnbrs) mesh->synchronize(nbr_enum);
  typename SIMPLEX::array_type nbrs;

  typename SIMPLEX::iterator si, sie;
  mesh->begin(si);
  mesh->end(sie);
  Point p;
  int row=0;
  while (si != sie) {
    int col=0;
    if (x || y || z) mesh->get_center(p, *si);
    if (x)     { m->get(row,col++) = p.x(); }
    if (y)     { m->get(row,col++) = p.y(); }
    if (z)     { m->get(row,col++) = p.z(); }
    if (idx)   { m->get(row,col++) = row; }
    if (nnbrs) { m->get(row,col++) = mesh->get_valence(*si); }
    if (size)  { m->get(row,col++) = mesh->get_size(*si); }
    ++si;
    row++;
  }
  return m;
}


}

#endif
