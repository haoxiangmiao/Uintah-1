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


/*
 *  ApplyFEMCurrentSource.cc: Builds the RHS of the FE matrix for
 *  current sources
 *
 *  Written by:
 *   David Weinstein
 *   University of Utah
 *   May 1999
 *  Modified by:
 *   Alexei Samsonov
 *   March 2001
 *  Copyright (C) 1999, 2001 SCI Group
 *
 *   Lorena Kreda, Northeastern University, November 2003
 */

#include <Core/Containers/Array1.h>
#include <Dataflow/Network/Module.h>
#include <Core/Datatypes/ColumnMatrix.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <Core/Datatypes/TetVolField.h>
#include <Core/Datatypes/HexVolField.h>
#include <Core/Datatypes/TriSurfField.h>
#include <Core/Datatypes/PointCloudField.h>
#include <Core/Datatypes/CurveField.h>
#include <Dataflow/Ports/MatrixPort.h>
#include <Dataflow/Ports/FieldPort.h>
#include <Dataflow/Widgets/BoxWidget.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Math/MinMax.h>
#include <Core/Math/Trig.h>
#include <Core/Math/MiscMath.h>
#include <Core/GuiInterface/GuiVar.h>
#include <iostream>
#include <Packages/BioPSE/Core/Algorithms/NumApproximation/ReferenceElement.h>

namespace BioPSE {

using namespace SCIRun;

class ApplyFEMCurrentSource : public Module {

  //! Input ports
  FieldIPort*   iportField_;
  FieldIPort*   iportSource_;
  FieldIPort*   iportInterp_;
  MatrixIPort*  iportRhs_;
  MatrixIPort*  iportCurrentPattern_;
  MatrixIPort*  iportCurrentPatternIndex_;
  MatrixIPort*  iportElectrodeParams_;
  FieldIPort*   iportFieldBoundary_;
  MatrixIPort*  iportBoundaryToMesh_;

  int gen_;
  TetVolMesh::Cell::index_type loc;
  TetVolMesh::Face::index_type locTri;

  bool tet;
  bool hex;
  bool tri;

  enum ElectrodeModelType
    {
      CONTINUUM_MODEL = 0,
      GAP_MODEL
    };

private:
  void execute_dipole();

  void execute_sources_and_sinks();

  void execute_electrode_set();
  void ProcessTriElectrodeSet( ColumnMatrix* rhs, TriSurfMeshHandle hTriMesh );
  double CalcContinuumTrigCurrent(Point p, int index, int numBoundaryNodes);
  double ComputeTheta(Point);

public:
  GuiInt sourceNodeTCL_;
  GuiInt sinkNodeTCL_;
  GuiString modeTCL_;

  //! Constructor/Destructor
  ApplyFEMCurrentSource(GuiContext *context);
  virtual ~ApplyFEMCurrentSource();

  //! Public methods
  virtual void execute();
};

DECLARE_MAKER(ApplyFEMCurrentSource)


ApplyFEMCurrentSource::ApplyFEMCurrentSource(GuiContext *context)
  : Module("ApplyFEMCurrentSource", context, Filter, "Forward", "BioPSE"),
    sourceNodeTCL_(context->subVar("sourceNodeTCL")),
    sinkNodeTCL_(context->subVar("sinkNodeTCL")),
    modeTCL_(context->subVar("modeTCL"))
{
}


ApplyFEMCurrentSource::~ApplyFEMCurrentSource()
{
}


void
ApplyFEMCurrentSource::execute()
{
  if (modeTCL_.get() == "dipole")
  {
    execute_dipole();
  }
  else if (modeTCL_.get() == "sources and sinks")
  {
    execute_sources_and_sinks();
  }
  else if (modeTCL_.get() == "electrode set")
  {
    execute_electrode_set();
  }
  else
  {
    error("Unreachable code, bad mode.");
  }
}


void
ApplyFEMCurrentSource::execute_dipole()
{
  iportField_ = (FieldIPort *)get_iport("Mesh");
  iportSource_ = (FieldIPort *)get_iport("Dipole Sources");
  iportInterp_ = (FieldIPort *)get_iport("Interpolant");
  iportRhs_ = (MatrixIPort *)get_iport("Input RHS");

  MatrixOPort *oportRhs = (MatrixOPort *)get_oport("Output RHS");
  MatrixOPort *oportWeights = (MatrixOPort *)get_oport("Output Weights");

  //! Obtaining handles to computation objects
  FieldHandle hField;
  if (!iportField_->get(hField) || !hField.get_rep()) {
    error("Can't get handle to input mesh.");
    return;
  }

  TetVolMesh *hTetMesh = 0;
  HexVolMesh *hHexMesh = 0;
  TriSurfMesh *hTriMesh = 0;
  if ((hTetMesh = dynamic_cast<TetVolMesh*>(hField->mesh().get_rep())))
  {
    remark("Input is a 'TetVolField'");
  }
  else if ((hHexMesh = dynamic_cast<HexVolMesh*>(hField->mesh().get_rep())))
  {    
    remark("Input is a 'HexVolField'");
  }
  else if ((hTriMesh = dynamic_cast<TriSurfMesh*> (hField->mesh().get_rep())))
  {
    remark("Input is a 'TriSurfField'");
  }
  else
  {
    error("Supplied field is not 'TetVolField' or 'HexVolField' or 'TriSurfField'");
    return;
  }

  int nsize = 0;
  if (hTetMesh)
  {
    TetVolMesh::Node::size_type nsizeTet; hTetMesh->size(nsizeTet);
    nsize = nsizeTet;
  }
  else if (hHexMesh)
  {
    HexVolMesh::Node::size_type nsizeHex; hHexMesh->size(nsizeHex);
    nsize = nsizeHex;
  }
  else if (hTriMesh)
  {
    TriSurfMesh::Node::size_type nsizeTri; hTriMesh->size(nsizeTri);
    nsize = nsizeTri;
  }

  if (nsize <= 0)
  {
    error("Input mesh has zero size");
    return;
  }

  // If the user passed in a vector the right size, copy it into ours.
  ColumnMatrix* rhs = 0;
  MatrixHandle  hRhsIn;
  if (iportRhs_->get(hRhsIn) && hRhsIn.get_rep())
  {
    if (hRhsIn->nrows() == nsize && hRhsIn->ncols() == 1)
    {
      rhs = scinew ColumnMatrix(nsize);
      string units;
      if (hRhsIn->get_property("units", units))
        rhs->set_property("units", units, false);

      for (int i=0; i < nsize; i++)
      {
        rhs->put(i, hRhsIn->get(i, 1));
      }
    }
    else
    {
      warning("The supplied RHS doesn't correspond to the input mesh in size.  Creating empty one.");
    }
  }
  if (rhs == 0)
  {
    rhs = scinew ColumnMatrix(nsize);
    rhs->set_property("units", string("volts"), false);
    rhs->zero();
  }

  // Process mesh.
  if (hTetMesh)
  {
    FieldHandle hSource;
    if (!iportSource_->get(hSource) || !hSource.get_rep()) {
      error("Can't get handle to source field.");
      return;
    }
	
    LockingHandle<PointCloudField<Vector> > hDipField;
	
    if (hSource->get_type_name(0)!="PointCloudField" ||
        hSource->get_type_name(1)!="Vector")
    {
      error("Supplied field is not of type PointCloudField<Vector>.");
      return;
    }
    else
    {
      hDipField = dynamic_cast<PointCloudField<Vector>*> (hSource.get_rep());
    }
	
    hTetMesh->synchronize(Mesh::LOCATE_E);
	
    //! Computing contributions of dipoles to RHS
    PointCloudMesh::Node::iterator ii;
    PointCloudMesh::Node::iterator ii_end;
    Array1<double> weights;
    hDipField->get_typed_mesh()->begin(ii);
    hDipField->get_typed_mesh()->end(ii_end);
    for (; ii != ii_end; ++ii)
    {
      Vector dir = hDipField->value(*ii); // correct unit ???
      Point p;
      hDipField->get_typed_mesh()->get_point(p, *ii);
		
      if (hTetMesh->locate(loc, p))
      {
        msgStream_ << "Source p="<<p<<" dir="<<dir<<" found in elem "<<loc<<endl;
        if (fabs(dir.x()) > 0.000001)
        {
          weights.add(loc*3);
          weights.add(dir.x());
        }
        if (fabs(dir.y()) > 0.000001)
        {
          weights.add(loc*3+1);
          weights.add(dir.y());
        }
        if (fabs(dir.z()) > 0.000001)
        {
          weights.add(loc*3+2);
          weights.add(dir.z());
        }
		
        double s1, s2, s3, s4;
        Vector g1, g2, g3, g4;
        hTetMesh->get_gradient_basis(loc, g1, g2, g3, g4);
		
        s1=Dot(g1,dir);
        s2=Dot(g2,dir);
        s3=Dot(g3,dir);
        s4=Dot(g4,dir);
		
        TetVolMesh::Node::array_type cell_nodes;
        hTetMesh->get_nodes(cell_nodes, loc);
        (*rhs)[cell_nodes[0]]+=s1;
        (*rhs)[cell_nodes[1]]+=s2;
        (*rhs)[cell_nodes[2]]+=s3;
        (*rhs)[cell_nodes[3]]+=s4;
      }
      else
      {
        msgStream_ << "Dipole: "<< p <<" not located within mesh!"<<endl;
      }
    }
    gen_=hSource->generation;
    ColumnMatrix* w = scinew ColumnMatrix(weights.size());
    for (int i=0; i<weights.size(); i++) (*w)[i]=weights[i];

    oportWeights->send(MatrixHandle(w));
    oportRhs->send(MatrixHandle(rhs));
  }
  else if (hHexMesh)
  { // process hex mesh
    FieldHandle hSource;
    if (!iportSource_->get(hSource) || !hSource.get_rep()) {
      error("Can't get handle to source field.");
      return;
    }
    LockingHandle<PointCloudField<Vector> > hDipField;
    if (hSource->get_type_name(0)!="PointCloudField" ||
        hSource->get_type_name(1)!="Vector")
    {
      error("Supplied field is not of type PointCloudField<Vector>.");
      return;
    }
    else {
      hDipField = dynamic_cast<PointCloudField<Vector>*> (hSource.get_rep());
    }

    hHexMesh->synchronize(Mesh::LOCATE_E);
	
    //! Computing contributions of dipoles to RHS
    PointCloudMesh::Node::iterator ii;
    PointCloudMesh::Node::iterator ii_end;
    Array1<double> weights;
    hDipField->get_typed_mesh()->begin(ii);
    hDipField->get_typed_mesh()->end(ii_end);
    HexVolMesh::Cell::index_type ci;
    ReferenceElement *rE_ = scinew ReferenceElement();
    for (; ii != ii_end; ++ii)
    { // loop over dipoles
      // Correct unit of dipole moment -> should be checked.
      Vector moment = hDipField->value(*ii);
      Point dipole;
      // Position of the dipole.
      hDipField->get_typed_mesh()->get_point(dipole, *ii);
      if (hHexMesh->locate(ci, dipole))
      {
        msgStream_ << "Source p="<<dipole<<" dir="<< moment <<
          " found in elem "<< loc <<endl;
      }
      else
      {
        msgStream_ << "Dipole: "<< dipole <<
          " not located within mesh!"<<endl;
      }

      // Get dipole in reference element.
      double xa, xb, ya, yb, za, zb;
      Point p;
      HexVolMesh::Node::array_type n_array;
      hHexMesh->get_nodes(n_array, ci);
      hHexMesh->get_point(p, n_array[0]);
      xa = p.x(); ya = p.y(); za = p.z();
      hHexMesh->get_point(p, n_array[6]);
      xb = p.x(); yb = p.y(); zb = p.z();
      Point diRef(rE_->isp1(dipole.x(), xa, xb),
                  rE_->isp2(dipole.y(), ya, yb),
                  rE_->isp3(dipole.z(), za, zb));

      // Update rhs
      for (int i=0; i <8; i++)
      {
        double val = moment[0] * rE_->dphidx(i, diRef.x(), diRef.y(), diRef.z()) / rE_->dpsi1dx(xa, xb)
          + moment[1] * rE_->dphidy(i, diRef.x(), diRef.y(), diRef.z()) / rE_->dpsi2dy(ya, yb)
          + moment[2] * rE_->dphidz(i, diRef.x(), diRef.y(), diRef.z()) / rE_->dpsi3dz(za,zb);
        rhs->put((int)n_array[i], val);
      }
    }
    oportRhs->send(MatrixHandle(rhs));
  }
  else if (hTriMesh)
  {
    FieldHandle hSource;
    if (!iportSource_->get(hSource) || !hSource.get_rep()) {
      error("Can't get handle to source field.");
      return;
    }

    LockingHandle<PointCloudField<Vector> > hDipField;

    if (hSource->get_type_name(0)!="PointCloudField" ||
        hSource->get_type_name(1)!="Vector")
    {
      error("Supplied field is not of type PointCloudField<Vector>.");
      return;
    }
    else
    {
      hDipField = dynamic_cast<PointCloudField<Vector>*> (hSource.get_rep());
    }

    //! Computing contributions of dipoles to RHS
    PointCloudMesh::Node::iterator ii;
    PointCloudMesh::Node::iterator ii_end;
    Array1<double> weights;
    hDipField->get_typed_mesh()->begin(ii);
    hDipField->get_typed_mesh()->end(ii_end);
    for (; ii != ii_end; ++ii)
    {

      Vector dir = hDipField->value(*ii);
      Point p;
      hDipField->get_typed_mesh()->get_point(p, *ii);

      if (hTriMesh->locate(locTri, p))
      {
        msgStream_ << "Source p="<<p<<" dir="<<dir<<
          " found in elem "<< locTri <<endl;

        if (fabs(dir.x()) > 0.000001)
        {
          weights.add(locTri*3);
          weights.add(dir.x());
        }
        if (fabs(dir.y()) > 0.000001)
        {
          weights.add(locTri*3+1);
          weights.add(dir.y());
        }
        if (fabs(dir.z()) > 0.000001)
        {
          weights.add(locTri*3+2);
          weights.add(dir.z());
        }
	
        double s1, s2, s3;
        Vector g1, g2, g3;
        hTriMesh->get_gradient_basis(locTri, g1, g2, g3);

        s1=Dot(g1,dir);
        s2=Dot(g2,dir);
        s3=Dot(g3,dir);
		
        TriSurfMesh::Node::array_type face_nodes;
        hTriMesh->get_nodes(face_nodes, locTri);
        (*rhs)[face_nodes[0]]+=s1;
        (*rhs)[face_nodes[1]]+=s2;
        (*rhs)[face_nodes[2]]+=s3;
      }
      else
      {
        msgStream_ << "Dipole: "<< p <<" not located within mesh!"<<endl;
      }
    } // end for loop
    gen_=hSource->generation;
    ColumnMatrix* w = scinew ColumnMatrix(weights.size());
    for (int i=0; i<weights.size(); i++) (*w)[i]=weights[i];

    oportWeights->send(MatrixHandle(w));
    oportRhs->send(MatrixHandle(rhs));
  }
}


void
ApplyFEMCurrentSource::execute_sources_and_sinks()
{
  iportField_ = (FieldIPort *)get_iport("Mesh");
  iportSource_ = (FieldIPort *)get_iport("Dipole Sources");
  iportInterp_ = (FieldIPort *)get_iport("Interpolant");
  iportRhs_ = (MatrixIPort *)get_iport("Input RHS");

  MatrixOPort *oportRhs = (MatrixOPort *)get_oport("Output RHS");

  //! Obtaining handles to computation objects
  FieldHandle hField;
  if (!iportField_->get(hField) || !hField.get_rep()) {
    error("Can't get handle to input mesh.");
    return;
  }

  TetVolMesh *hTetMesh = 0;
  TriSurfMesh *hTriMesh = 0;
  if ((hTetMesh = dynamic_cast<TetVolMesh*>(hField->mesh().get_rep())))
  {
    remark("Input is a 'TetVolField'");
  }
  else if ((hTriMesh = dynamic_cast<TriSurfMesh*> (hField->mesh().get_rep())))
  {
    remark("Input is a 'TriSurfField'");
  }
  else
  {
    error("Supplied field is not 'TetVolField' or 'TriSurfField'");
    return;
  }

  int nsize = 0;
  if (hTetMesh)
  {
    TetVolMesh::Node::size_type nsizeTet; hTetMesh->size(nsizeTet);
    nsize = nsizeTet;
  }
  else if (hTriMesh)
  {
    TriSurfMesh::Node::size_type nsizeTri; hTriMesh->size(nsizeTri);
    nsize = nsizeTri;
  }

  if (nsize <= 0)
  {
    error("Input mesh has zero size");
    return;
  }

  // If the user passed in a vector the right size, copy it into ours.
  ColumnMatrix* rhs = 0;
  MatrixHandle  hRhsIn;
  if (iportRhs_->get(hRhsIn) && hRhsIn.get_rep())
  {
    if (hRhsIn->ncols() == 1 && hRhsIn->nrows() == nsize)
    {
      rhs = scinew ColumnMatrix(nsize);
      string units;
      if (hRhsIn->get_property("units", units))
        rhs->set_property("units", units, false);

      for (int i=0; i < nsize; i++)
      {
        rhs->put(i, hRhsIn->get(i, 1));
      }
    }
    else
    {
      warning("The supplied RHS doesn't correspond to the input mesh in size.  Creating empty one.");
    }
  }
  if (rhs == 0)
  {
    rhs = scinew ColumnMatrix(nsize);
    rhs->set_property("units", string("volts"), false);
    rhs->zero();
  }

  // process mesh
  if (hTetMesh)
  {
    FieldHandle hInterp;
    iportInterp_->get(hInterp);
    FieldHandle hSource;
    iportSource_->get(hSource);
	
    unsigned int sourceNode = Max(sourceNodeTCL_.get(), 0);
    unsigned int sinkNode = Max(sinkNodeTCL_.get(), 0);
	
    // if we have an Interp field and its type is good, hInterpTetToPC will
    //  be valid after this block
    LockingHandle<PointCloudField<vector<pair<TetVolMesh::Node::index_type, double> > > > hInterpTetToPC;
    if (hInterp.get_rep())
    {
      hInterpTetToPC = dynamic_cast<PointCloudField<vector<pair<TetVolMesh::Node::index_type, double> > > *>(hInterp.get_rep());
      if (!hInterpTetToPC.get_rep())
      {
        error("Input interp field wasn't interp'ing PointCloudField from a TetVolMesh::Node.");
        return;
      }
    }
	
    // if we have an Interp field and a Source field and all types are good,
    //  hCurField will be valid after this block
    LockingHandle<PointCloudField<double> > hCurField;
    if (hInterpTetToPC.get_rep() && hSource.get_rep())
    {
      if (hSource->get_type_name(0)=="PointCloudField")
      {
        if (hSource->get_type_name(1)!="double")
        {
          error("Can only use a PointCloudField<double> when using an Interp field and a source field -- this mode is for specifying current densities");
          return;
        }
        hCurField = dynamic_cast<PointCloudField<double>*> (hSource.get_rep());
        if (hInterpTetToPC->get_typed_mesh().get_rep() !=
            hCurField->get_typed_mesh().get_rep())
        {
          error("Can't have different meshes for the Source and Interp field");
          return;
        }
      }
      else
      {
        error("Can only use a PointCloudField<double> for the current sources");
        return;
      }
    }
	
    // if we have don't have an Interp field, use the source/sink indices
    //  directly as TetVol nodes
	
    // if we do have an Interp field, but we don't have a Source field,
    //  then the source/sink indices refer to the PointCloud, so use the
    //  InterpField to get their corresponding TetVol node indices
	
    // if we have an Interp field AND a Source field, then ignore the
    //  source/sink indices.  The Source field and the Interp field
    //  will have the same mesh, where the Interp field speifies the
    //  TetVol node index for each source, and the Source field gives a
    //  scalar quantity (current) for each source
    if (!hInterpTetToPC.get_rep())
    {
      if ((int)sourceNode >= nsize || (int)sinkNode >= nsize)
      {
        error("SourceNode or SinkNode was out of mesh range.");
        return;
      }
      (*rhs)[sourceNode] += -1;
      (*rhs)[sinkNode] += 1;
      oportRhs->send(MatrixHandle(rhs));
      return;
    }
	
    if (!hCurField.get_rep())
    {
      if (sourceNode < hInterpTetToPC->fdata().size() &&
          sinkNode < hInterpTetToPC->fdata().size())
      {
        sourceNode = hInterpTetToPC->fdata()[sourceNode].begin()->first;
        sinkNode = hInterpTetToPC->fdata()[sinkNode].begin()->first;
      }
      else
      {
        error("SourceNode or SinkNode was out of interp range.");
        return;
      }
      (*rhs)[sourceNode] += -1;
      (*rhs)[sinkNode] += 1;
      oportRhs->send(MatrixHandle(rhs));
      return;
    }
	
    PointCloudMesh::Node::iterator ii;
    PointCloudMesh::Node::iterator ii_end;
    Array1<double> weights;
    hInterpTetToPC->get_typed_mesh()->begin(ii);
    hInterpTetToPC->get_typed_mesh()->end(ii_end);
    for (; ii != ii_end; ++ii)
    {
      vector<pair<TetVolMesh::Node::index_type, double> > vp;
      hInterpTetToPC->value(vp, *ii);
      double currentDensity;
      hCurField->value(currentDensity, *ii);
      for (unsigned int vv=0; vv<vp.size(); vv++)
      {
        unsigned int rhsIdx = (unsigned int)(vp[vv].first);
        double rhsVal = vp[vv].second * currentDensity;
        (*rhs)[rhsIdx] += rhsVal;
      }
    }
    oportRhs->send(MatrixHandle(rhs));
  }
  else if (hTriMesh)
  {
    FieldHandle hInterp;
    iportInterp_->get(hInterp);
    unsigned int sourceNode = Max(sourceNodeTCL_.get(), 0);
    unsigned int sinkNode = Max(sinkNodeTCL_.get(), 0);

    if (hInterp.get_rep())
    {
      PointCloudField<vector<pair<TriSurfMesh::Node::index_type, double> > >* interp;
      interp = dynamic_cast<PointCloudField<vector<pair<TriSurfMesh::Node::index_type, double> > > *>(hInterp.get_rep());
      if (!interp)
      {
        error("Input interp field wasn't interp'ing PointCloudField from a TriSurfMesh::Node.");
        return;
      }
      else if (sourceNode < interp->fdata().size() &&
               sinkNode < interp->fdata().size())
      {
        sourceNode = interp->fdata()[sourceNode].begin()->first;
        sinkNode = interp->fdata()[sinkNode].begin()->first;
      }
      else
      {
        error("SourceNode or SinkNode was out of interp range.");
        return;
      }
    }
    if (sourceNode >= (unsigned int) nsize ||
        sinkNode >= (unsigned int) nsize)
    {
      error("SourceNode or SinkNode was out of mesh range.");
      return;
    }
    msgStream_ << "sourceNode="<<sourceNode<<" sinkNode="<<sinkNode<<"\n";
    (*rhs)[sourceNode] += -1;
    (*rhs)[sinkNode] += 1;

    //! Sending result
    oportRhs->send(MatrixHandle(rhs));
  }
}


void
ApplyFEMCurrentSource::execute_electrode_set()
{
  iportField_ = (FieldIPort *)get_iport("Mesh");
  iportSource_ = (FieldIPort *)get_iport("Dipole Sources");
  iportInterp_ = (FieldIPort *)get_iport("Interpolant");
  iportRhs_ = (MatrixIPort *)get_iport("Input RHS");
  iportCurrentPattern_ = (MatrixIPort *)get_iport("Current Pattern");
  iportCurrentPatternIndex_ = (MatrixIPort *)get_iport("CurrentPatternIndex");
  iportElectrodeParams_ = (MatrixIPort *)get_iport("Electrode Parameters");
  iportFieldBoundary_ = (FieldIPort *)get_iport("Boundary");
  iportBoundaryToMesh_ = (MatrixIPort *)get_iport("Boundary Transfer Matrix");

  MatrixOPort *oportRhs = (MatrixOPort *)get_oport("Output RHS");

  //! Obtaining handles to computation objects
  FieldHandle hField;
  if (!iportField_->get(hField) || !hField.get_rep()) {
    error("Can't get handle to input mesh.");
    return;
  }

  TriSurfMesh *hTriMesh = 0;
  if ((hTriMesh = dynamic_cast<TriSurfMesh*> (hField->mesh().get_rep())))
  {
    remark("Input is a 'TriSurfField'");
  }
  else
  {
    error("Only TriSurfField type is supported in electrode set mode");
    return;
  }

  TriSurfMesh::Node::size_type nsizeTri; hTriMesh->size(nsizeTri);
  const int nsize = nsizeTri;
  if (nsize <= 0)
  {
    error("Input mesh has zero size");
    return;
  }

  // If the user passed in a vector the right size, copy it into ours.
  ColumnMatrix* rhs = 0;
  MatrixHandle  hRhsIn;
  if (iportRhs_->get(hRhsIn) && hRhsIn.get_rep())
  {
    if (hRhsIn->ncols() == 1 && hRhsIn->nrows() == nsize)
    {
      rhs = scinew ColumnMatrix(nsize);
      string units;
      if (hRhsIn->get_property("units", units))
        rhs->set_property("units", units, false);

      for (int i=0; i < nsize; i++)
      {
        rhs->put(i, hRhsIn->get(i, 1));
      }
    }
    else
    {
      warning("The supplied RHS doesn't correspond to the input mesh in size.  Creating empty one.");
    }
  }
  if (rhs == 0)
  {
    rhs = scinew ColumnMatrix(nsize);
    rhs->set_property("units", string("volts"), false);
    rhs->zero();
  }

  ProcessTriElectrodeSet( rhs, hTriMesh );
  
  //! Sending result
  oportRhs->send(MatrixHandle(rhs));
}


// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ApplyFEMCurrentSource::ProcessTriElectrodeSet
//
// Description: This method isolates a specialized block of code that
// handles the TriSurfMesh and 'Electrode Set' mode.
//
// Inputs:
//
// Returns:
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void
ApplyFEMCurrentSource::ProcessTriElectrodeSet( ColumnMatrix* rhs,
                                               TriSurfMeshHandle hTriMesh )
{
  int numParams=4;

  // Get the electrode parameters input vector
  // -----------------------------------------
  MatrixHandle  hElectrodeParams;

  if (!iportElectrodeParams_->get(hElectrodeParams) ||
      !hElectrodeParams.get_rep())
  {
    error("Can't get handle to electrode parameters matrix.");
    return;
  }

  ColumnMatrix* electrodeParams = scinew ColumnMatrix(numParams);
  electrodeParams=dynamic_cast<ColumnMatrix*>(hElectrodeParams.get_rep());

  unsigned int electrodeModel = (unsigned int)((*electrodeParams)[0]);
  int numElectrodes           = (int) ( (*electrodeParams)[1]);
  double electrodeLen         = (*electrodeParams)[2];
  int startNodeIndex          = (int) ( (*electrodeParams)[3]);

  // Get the current pattern input
  // -----------------------------
  // These are the input currents at each electrode - later we will
  // combine with electrode information to produce an electrode
  // density. In 2D, we assume the electrode height is 1 because we
  // assume no variation in the z direction, hence we set h=1 so that
  // it doesn't influence the computation.  This input is used only
  // for models other than the continuum model

  MatrixHandle  hCurrentPattern;
  if ((!iportCurrentPattern_->get(hCurrentPattern) ||
       !hCurrentPattern.get_rep()) && (electrodeModel != CONTINUUM_MODEL))
  {
    error("Can't get handle to current pattern matrix.");
    return;
  }

  // Get the current pattern index
  // -----------------------------
  // This is used for calculating the current value if the continuum
  // model is used
  MatrixHandle  hCurrentPatternIndex;
  ColumnMatrix* currPatIdx;
  int           k = 0;

  // Copy the input current index into local variable, k
  // ---------------------------------------------------
  if (iportCurrentPatternIndex_->get(hCurrentPatternIndex) &&
      (currPatIdx=dynamic_cast<ColumnMatrix*>(hCurrentPatternIndex.get_rep())) &&
      (currPatIdx->nrows() == 1))
  {
    k=static_cast<int>((*currPatIdx)[0]);
  }
  else
  {
    msgStream_ << "The supplied current pattern index is not a 1x1 matrix" << endl;
  }

  // Get the FieldBoundary input
  // ---------------------------
  FieldHandle      hFieldBoundary;
  CurveMeshHandle  hBoundaryMesh;
  LockingHandle<CurveField<double> > hCurveBoundary;
  bool boundary = false;
  if ( iportFieldBoundary_->get(hFieldBoundary) )
  {
    if (hFieldBoundary.get_rep())
    {
      // Check field type - this only works for CurveFields<double>
      // extracted from a TriSurf
      if ( (hFieldBoundary->get_type_name(0) == "CurveField") &&
           (hFieldBoundary->get_type_name(1) == "double") )
      {
        remark("Field boundary input is a CurveField<double>");
        hCurveBoundary = dynamic_cast<CurveField<double>*> ( hFieldBoundary.get_rep() );
        hBoundaryMesh = hCurveBoundary->get_typed_mesh();
        boundary = true;
      }
      else
      {
        remark("Supplied boundary field is not of type CurveField<double>");
      }
    }
  }
  else
  {
    msgStream_ << "There is an error in the supplied boundary field" << endl;
  }

  // If a boundary field was supplied, check for the matrix that maps
  // boundary nodes to mesh nodes. This matrix is the output of the
  // module: InterpolantToTransferMatrix
  MatrixHandle      hBoundaryToMesh;

  if (boundary)
  {
    if ( !(iportBoundaryToMesh_->get(hBoundaryToMesh) &&
           hBoundaryToMesh.get_rep()) )
    {
      // disable susequent boundary-related code if we had a problem here
      boundary = false;
    }
  }

  // Get the interp field
  // --------------------
  // This is the location of the electrodes interpolated onto the body
  // mesh. The presence of this input means the user is electing to
  // specify electrode locations manually rather than use an automatic
  // placement scheme selected through the electrode manager.
  FieldHandle hInterp;

  if ( iportInterp_->get(hInterp) && hInterp.get_rep())
  {
    // Determine the dimension (number of electrodes) in the interp'd
    // field of electrodes.
    PointCloudMesh::Node::size_type nsize;
    LockingHandle<PointCloudField<vector<pair<NodeIndex<int>,double> > > >hInterpField;
    PointCloudMeshHandle hPtCldMesh;
    PointCloudField<vector<pair<NodeIndex<int>,double> > >* interp =
      dynamic_cast<PointCloudField<vector<pair<NodeIndex<int>,double> > > *> (hInterp.get_rep());
    hPtCldMesh = interp->get_typed_mesh();
    hPtCldMesh->size(nsize);
  }

  // If electrode interp field is not supplied, determine electrode
  // centers using number of electrodes, spacing from the electrode
  // manager and extracted field boundary
  else
  {
  }

  // Make a local copy of the input current pattern Hold off on
  // copying the current pattern until after we check if there's an
  // interpolated electrode field as this could influence the value of
  // numElectrodes Also, this input is not needed for the continuum
  // case and may not be present in this case.
  ColumnMatrix* currentPattern = scinew ColumnMatrix(numElectrodes);
  currentPattern=dynamic_cast<ColumnMatrix*>(hCurrentPattern.get_rep());

  // Allocate vector for the mesh-to-electrode-map
  ColumnMatrix* meshToElectrodeMap;
  TriSurfMesh::Node::size_type msize;
  hTriMesh->size(msize);
  int numMeshNodes = (int) msize;

  meshToElectrodeMap = scinew ColumnMatrix(msize);

  // Initialize meshToElectrodeMap to all -1s. -1 indicates a
  // non-electrode node; later we will identify the electrode nodes.
  for (int i = 0; i < numMeshNodes; i++)
  {
    (*meshToElectrodeMap)[i] = -1;
  }

  // TRI + ELECTRODE SET + CONTINUUM MODEL
  // -------------------------------------
  if (electrodeModel == CONTINUUM_MODEL)
  {
    if (boundary)
    {
      // Visit each node on the boundary mesh.
      CurveMesh::Node::iterator nodeItr;
      CurveMesh::Node::iterator nodeItrEnd;
  	
      hBoundaryMesh->begin(nodeItr);
      hBoundaryMesh->end(nodeItrEnd);

      Array1<int>       meshNodeIndex;
      Array1<double>    weight;

      int numBoundaryNodes = hBoundaryToMesh->nrows();

      for (; nodeItr != nodeItrEnd; ++nodeItr)
      {		
        Point p;
        hBoundaryMesh->get_point(p, *nodeItr);

        // Find the corresponding node index in the body (TriSurf) mesh.
        hBoundaryToMesh->getRowNonzeros(*nodeItr, meshNodeIndex, weight);
		
        int rhsIndex = meshNodeIndex[0];

        // Get the value for the current at this node and store this
        // value in the RHS output vector
        (*rhs)[rhsIndex] = CalcContinuumTrigCurrent(p, k, numBoundaryNodes);

        // Tag this node as an "electrode" node
        (*meshToElectrodeMap)[rhsIndex] = (*nodeItr);
      }
    } // end if (boundary)
  } // end else (if model == CONTINUUM_MODEL)
  // TRI + ELECTRODE SET + GAP MODEL
  // -------------------------------
  else if (electrodeModel == GAP_MODEL )
  {
    // Originally, we didn't execute if an electrode interp field was
    // not supplied because this is the only way we know where the
    // electrodes are on the input mesh.  Supplying a point cloud
    // field of electrode positions could still be an option, but it
    // is not supported now.  The equivalent effect can be obtained
    // using the ElectrodeManager module.  The hInterp input is
    // ignored by this part of ApplyFEMCurrentSource.

    // The code below places electrodes on the boundary of the input field.

    // Traverse the boundary (curve) field and determine its length
    if (!boundary)
    {
      error("Cannot proceed without a field boundary");
      return;
    }

    // Iterate over edges in the boundary and build a look-up-table
    // that maps each node index to its neighbor node indices.
    CurveMesh::Node::size_type nsize;
    hBoundaryMesh->size(nsize);
    int numBoundaryNodes = (int) nsize;

    Array1<Array1<CurveMesh::Node::index_type> > neighborNodes;
    neighborNodes.resize(numBoundaryNodes);

    Array1<Array1<CurveMesh::Edge::index_type> > neighborEdges;
    neighborEdges.resize(numBoundaryNodes);

    Array1<double> edgeLength;
    edgeLength.resize(numBoundaryNodes);

    CurveMesh::Node::array_type childNodes;

    CurveMesh::Edge::iterator edgeItr;
    CurveMesh::Edge::iterator edgeItrEnd;

    hBoundaryMesh->begin(edgeItr);
    hBoundaryMesh->end(edgeItrEnd);

    double boundaryLength = 0.0;

    for (; edgeItr != edgeItrEnd; ++edgeItr)
    {		
      hBoundaryMesh->get_nodes(childNodes, *edgeItr);
      unsigned int nodeIndex0 = (unsigned int) childNodes[0];
      unsigned int nodeIndex1 = (unsigned int) childNodes[1];

      neighborNodes[nodeIndex0].add(nodeIndex1);
      neighborNodes[nodeIndex1].add(nodeIndex0);
      neighborEdges[nodeIndex0].add(*edgeItr);
      neighborEdges[nodeIndex1].add(*edgeItr);

      // Store the edge length for future reference
      edgeLength[(unsigned int) *edgeItr] = hBoundaryMesh->get_size(*edgeItr);

      // Accumulate the total boundary length
      boundaryLength += edgeLength[(unsigned int) *edgeItr];

    }

    double electrodeSeparation = boundaryLength / numElectrodes;

    // Using the map we just created (neighborNodes), traverse the
    // boundary and assign electrode nodes Create an array that maps
    // boundary node index to electrode index. Initialize this array
    // to -1's meaning each boundary node is not assigned to an
    // electrode. A boundary node may only belong to one electrode.

    Array1<int> nodeElectrodeMap;
    nodeElectrodeMap.resize(numBoundaryNodes);
    for (int i = 0; i < numBoundaryNodes; i++)
    {
      nodeElectrodeMap[i] = -1;
    }

    Array1<Array1<bool> > nodeFlags;
    nodeFlags.resize(numBoundaryNodes);
    for (int i = 0; i < numBoundaryNodes; i++)
    {
      nodeFlags[i].resize(2);
      for (int j = 0; j < 2; j++)
      {
        nodeFlags[i][j] = false;
      }
    }

    Array1<Array1<double> > adjacentEdgeLengths;
    adjacentEdgeLengths.resize(numBoundaryNodes);
    for (int i = 0; i < numBoundaryNodes; i++)
    {
      adjacentEdgeLengths[i].resize(2);
      for (int j = 0; j < 2; j++)
      {
        adjacentEdgeLengths[i][j] = 0.0;
      }
    }

    // Let the node in the boundary mesh given by startNodeIndex (in
    // the electrodeParams input) be the first node in the first
    // electrode.
    int prevNode = -1;
    int currNode = startNodeIndex;
    int nextNode = neighborNodes[currNode][1];
    // Selecting element [0] or [1] influences the direction in which
    // we traverse the boundary (this should be investigated; [1]
    // seems to work well relative to the analytic solution.

    double cumulativeElectrodeLength = 0.0;
    double cumulativeElectrodeSeparation = 0.0;

    bool done = false;

    // Maximum error we can accept = 1/2 avg. edge length.
    double maxError = boundaryLength/numBoundaryNodes/2;
    // Abs difference between a desired length and a current cumulative length.
    double currError = 0.0;

    // Index of the boundary edge currently being considered.
    int currEdgeIndex = 0;

    // Flag to indicate this is the first node in an electrode
    bool firstNode = true;

    for (int i = 0; i < numElectrodes; i++)
    {
      while (!done)
      {
        // Label the current node with the current electrode ID
        if (nodeElectrodeMap[currNode] == -1)
        {
          nodeElectrodeMap[currNode] = i;
        }

        if (firstNode)
        {
          nodeFlags[currNode][0] = true;
          firstNode = false;
        }

        // Traverse the boundary until distance closest to the desired
        // electrode length is achieved.

        // First, determine if this is the degenerate 1-node electrode case
        // ----------------------------------------------------------------
        if (electrodeLen <= maxError)
        {
          nodeFlags[currNode][1] = true;  // the current node is the last node
          done = true;
          cumulativeElectrodeLength = 0.0;
        }

        // Find the index of the edge between currNode and nextNode
        // --------------------------------------------------------
        int candidateEdgeIndex0 = neighborEdges[currNode][0];
        int candidateEdgeIndex1 = neighborEdges[currNode][1];

        if ((int) neighborEdges[nextNode][0] == candidateEdgeIndex0 )
        {
          currEdgeIndex = candidateEdgeIndex0;
        }
        else if ((int) neighborEdges[nextNode][1] == candidateEdgeIndex0 )
        {
          currEdgeIndex = candidateEdgeIndex0;
        }
        else if ((int) neighborEdges[nextNode][0] == candidateEdgeIndex1 )
        {
          currEdgeIndex = candidateEdgeIndex1;
        }
        else if ((int) neighborEdges[nextNode][1] == candidateEdgeIndex1 )
        {
          currEdgeIndex = candidateEdgeIndex1;
        }

        // For first nodes that are not also last nodes, store the
        // forward direction adjacent edge length
        if (nodeFlags[currNode][1] != true)
        {
          adjacentEdgeLengths[currNode][1] = edgeLength[currEdgeIndex];
        }

        // Handle case where electrode covers more than one node
        if (!done)
        {
          // Determine if it is better to include the next node or the
          // next two nodes (If the effective electrode length will be
          // closer to the desired electrode length.)
          double testLength1 = cumulativeElectrodeLength
            + edgeLength[currEdgeIndex];
          double testError1 = Abs(electrodeLen - testLength1);

          // Advance along boundary to test addition of the next node.
          int tempPrevNode = currNode;
          int tempCurrNode = nextNode;
          int tempNextNode = -1;
          if ((int) neighborNodes[tempCurrNode][1] != tempPrevNode)
          {
            tempNextNode = (int) neighborNodes[tempCurrNode][1];
          }
          else
          {
            tempNextNode = (int) neighborNodes[tempCurrNode][0];
          }

          // Find the index of the edge between tempCurrNode and tempNextNode
          // ----------------------------------------------------------------
          int candidateEdgeIndex0 = neighborEdges[tempCurrNode][0];
          int candidateEdgeIndex1 = neighborEdges[tempCurrNode][1];

          int tempEdgeIndex = -1;

          if ((int) neighborEdges[tempNextNode][0] == candidateEdgeIndex0 )
          {
            tempEdgeIndex = candidateEdgeIndex0;
          }
          else if ((int) neighborEdges[tempNextNode][1] == candidateEdgeIndex0 )
          {
            tempEdgeIndex = candidateEdgeIndex0;
          }
          else if ((int) neighborEdges[tempNextNode][0] == candidateEdgeIndex1 )
          {
            tempEdgeIndex = candidateEdgeIndex1;
          }
          else if ((int) neighborEdges[tempNextNode][1] == candidateEdgeIndex1 )
          {
            tempEdgeIndex = candidateEdgeIndex1;
          }

          double testLength2 = testLength1 + edgeLength[tempEdgeIndex];
          double testError2 = Abs(electrodeLen - testLength2);

          if (testError1 < testError2)
          {
            // This means the nearer node achieves an electrode length
            // closer to that desired and that this node is the last
            // node in the electrode.
            nodeElectrodeMap[nextNode] = i;
            nodeFlags[nextNode][1] = true;
            cumulativeElectrodeLength = testLength1;
  	
            // We also need to store the backward direction adjacent
            // edge length for nextNode
            adjacentEdgeLengths[nextNode][0] = edgeLength[currEdgeIndex];

            done = true;
          }
          else
          {
            // This means the further node achieves an electrode
            // length closer to that desired.
            nodeElectrodeMap[nextNode] = i;
            cumulativeElectrodeLength = testLength1;

            // For middle nodes, we need to store both the backward
            // and forward adjacent edge lengths for nextNode.
            adjacentEdgeLengths[nextNode][0] = edgeLength[currEdgeIndex];
            adjacentEdgeLengths[nextNode][1] = edgeLength[tempEdgeIndex];
          }

          // Advance node pointers whether the electrode stops or continues.
          prevNode = tempPrevNode;
          currNode = tempCurrNode;
          nextNode = tempNextNode;
        } // end if (!done)
      }  // end while (!done)

      // At this point, we've finished with the current electrode.
      // Now we need to find the first node in the next electrode -
      // this will be based on the value of
      // cumulativeElectrodeSeparation which we can initialize here to
      // the value of cumulativeElectrodeLength.
      cumulativeElectrodeSeparation = cumulativeElectrodeLength;

      bool startNewElectrode = false;

      while (!startNewElectrode)
      {
        cumulativeElectrodeSeparation += edgeLength[currEdgeIndex];

        currError = Abs(electrodeSeparation - cumulativeElectrodeSeparation);

        if (currError <= maxError)
        {
          // We're within 1/2 an edge segment of the ideal electrode
          // separation.
          prevNode = currNode;
          currNode = nextNode;

          // Initialize nextNode.
          if ((int) neighborNodes[currNode][1] != prevNode)
          {
            nextNode = neighborNodes[currNode][1];
          }
          else
          {
            nextNode = neighborNodes[currNode][0];
          }

          startNewElectrode = true;
        }
        else if (cumulativeElectrodeSeparation > electrodeSeparation)
        {
          // The current error is greater than we allow, and we've
          // exceeded the separation we want.  We're trying to make
          // the first node in the next electrode equal to the last
          // node in the last electrode - this is not allowed
          error("Electrodes cannot overlap.");
          return;
        }
        // Otherwise, The current error is greater than 1/2 an edge
        // segment, and the cumulativeElectrodeSeparation is still
        // less than what we want. This happens when we have more than
        // one non-electrode node between electrodes.  do nothing in
        // this case.

        if (!startNewElectrode)
        {
          prevNode = currNode;
          currNode = nextNode;
          if ((int)neighborNodes[currNode][1] != prevNode)
          {
            nextNode = neighborNodes[currNode][1];
          }
          else
          {
            nextNode = neighborNodes[currNode][0];
          }
        }
      }  // end while (!startNewElectrode)

      done = false;
      firstNode = true;
      cumulativeElectrodeLength = 0.0;
      cumulativeElectrodeSeparation = 0.0;
    }

    // Determine the currents for the RHS vector
    // -----------------------------------------
    for (int i = 0; i < numBoundaryNodes; i++)
    {
      // Note: size of the currentPattern vector must be equal to the
      // number of electrodes!!  test this above
      if (nodeElectrodeMap[i] != -1 )  // this is an electrode node
      {
        double basisInt = 0.0;
        double current = 0.0;
        // Special case: single node electrode.
        if ( (nodeFlags[i][0] == 1) && (nodeFlags[i][1] == 1) )
        {
          current = (*currentPattern)[ nodeElectrodeMap[i] ];
        }
        // This is the first node in an electrode.
        else if (nodeFlags[i][0] == 1)
        {
          basisInt = 0.5 * adjacentEdgeLengths[i][1];
          current = basisInt * (*currentPattern)[ nodeElectrodeMap[i] ];
        }
        // This is the last node in an electrode.
        else if (nodeFlags[i][1] == 1)
        {
          basisInt = 0.5 * adjacentEdgeLengths[i][0];
          current = basisInt * (*currentPattern)[ nodeElectrodeMap[i] ];
        }
        else  // this is a middle node in an electrode
        {
          basisInt = 0.5 * adjacentEdgeLengths[i][0] +
            0.5 * adjacentEdgeLengths[i][1];
          current = basisInt * (*currentPattern)[ nodeElectrodeMap[i] ];
        }

        Array1<int>       meshNodeIndex;
        Array1<double>    weight;

        // Find the corresponding TriSurfMesh node index
        hBoundaryToMesh->getRowNonzeros(i, meshNodeIndex, weight);
	
        int rhsIndex = meshNodeIndex[0];

        (*rhs)[rhsIndex] = current;

        // Tag this node as an "electrode" node using the electrode index
        (*meshToElectrodeMap)[rhsIndex] = nodeElectrodeMap[i];
      }
    }
  } // end if GAP model

  //! Send the meshToElectrodeMap
  MatrixOPort *oportMeshToElectrodeMap =
    (MatrixOPort *)get_oport("Mesh to Electrode Map");
  oportMeshToElectrodeMap->send(MatrixHandle(meshToElectrodeMap));
}


// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ApplyFEMCurrentSource::CalcContinuumTrigCurrent
//
// Description:
//
// Inputs:
//
// Returns:
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

double
ApplyFEMCurrentSource::CalcContinuumTrigCurrent(Point p, int k,
                                                int numBoundaryNodes)
{
  double current;

  const double theta = ComputeTheta(p);

  if ( k < (numBoundaryNodes/2) + 1 )
  {
    current = cos(k*theta);
  }
  else
  {
    current = sin((k-numBoundaryNodes/2)*theta);
  }

  return current;
}


// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ApplyFEMCurrentSource::ComputeTheta
//
// Description: Find the angle, theta, the input point makes with the
// positive x axis.  This is a helper method for CalcContinuumTrigCurrent.
//
// Inputs:  Point p
//
// Returns: double theta, ( 0 <= theta < 2*PI )
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

double
ApplyFEMCurrentSource::ComputeTheta(Point p)
{
  double theta = 0.0;

  if ((p.x() <= 0) && (p.y() >= 0))
  {
    theta = Atan(p.y()/(p.x() + 0.0000000001)) + PI;
  }

  if ((p.x() <= 0) && (p.y() <= 0))
  {
    theta = Atan(p.y()/(p.x() + 0.0000000001)) + PI;
  }

  if ((p.x() >= 0) && (p.y() <= 0))
  {
    theta = Atan(p.y()/(p.x() + 0.0000000001)) + 2*PI;
  }

  if ((p.x() >= 0) && (p.y() >= 0))
  {
    theta = Atan(p.y()/(p.x() + 0.0000000001));
  }

  return theta;
}


} // End namespace BioPSE
