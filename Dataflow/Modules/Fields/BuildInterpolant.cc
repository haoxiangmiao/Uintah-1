/*
 *  BuildInterpolant.cc:  Build an interpolant field -- a field that says
 *         how to project the data from one field onto the data of a second
 *         field.
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   February 2001
 *
 *  Copyright (C) 2001 SCI Institute
 */

#include <Dataflow/Ports/MatrixPort.h>
#include <Dataflow/Ports/FieldPort.h>
#include <Core/Datatypes/SparseRowMatrix.h>
#include <Core/Datatypes/TriSurfMesh.h>
#include <Core/Malloc/Allocator.h>
#include <Core/GuiInterface/GuiVar.h>
#include <iostream>
using std::cerr;
#include <stdio.h>

namespace SCIRun {


class BuildInterpolant : public Module
{
public:

  BuildInterpolant(const clString& id);
  virtual ~BuildInterpolant();
  virtual void execute();

private:
  //! expects tet vol.
  FieldIPort     *imesh_;
  //! expects tri surf.
  FieldIPort     *isurf_;

  MatrixOPort    *omatrix_;
  MatrixHandle    omatrixH_;

  MatrixOPort    *omat_;
  MatrixHandle    omatH_;

  FieldOPort     *osurf_;
  FieldHandle     osurfH_;

  GuiString       method_;
  GuiInt          zeroTCL_;
  GuiInt          potMatTCL_;

  int             mesh_generation_;
  int             surf_generation_;
};


extern "C" Module* make_BuildInterpolant(const clString& id)
{
  return new BuildInterpolant(id);
}


BuildInterpolant::BuildInterpolant(const clString& id) : 
  Module("BuildInterpolant", id, Filter),
  method_("method", id, this),
  zeroTCL_("zeroTCL", id, this),
  potMatTCL_("potMatTCL", id, this)
{
  imesh_ = scinew FieldIPort(this, "Mesh", FieldIPort::Atomic);
  add_iport(imesh_);

  isurf_ = scinew FieldIPort(this, "Surface2", FieldIPort::Atomic);
  add_iport(isurf_);

  // Create the output ports.
  omat_ = scinew MatrixOPort(this, "Matrix", MatrixIPort::Atomic);
  add_oport(omat_);

  omatrix_ = scinew MatrixOPort(this, "Map", MatrixIPort::Atomic);
  add_oport(omatrix_);

  osurf_ = scinew FieldOPort(this, "NearestNodes", FieldIPort::Atomic);
  add_oport(osurf_);

  mesh_generation_ = -1;
  surf_generation_ = -1;
}


BuildInterpolant::~BuildInterpolant()
{
}


void
BuildInterpolant::execute()
{
  FieldHandle meshH;
  if (!imesh_->get(meshH))
    return;

  FieldHandle surfH;
  if (!isurf_->get(surfH))
    return;

  // TODO: Check gui state also.
  if (mesh_generation_ == meshH->generation &&
      surf_generation_ == surfH->generation &&
      omatrixH_.get_rep())
  {
    omatrix_->send(omatrixH_);
    omat_->send(omatH_);
    osurf_->send(osurfH_);
    return;
  }

  mesh_generation_ = meshH->generation;
  surf_generation_ = surfH->generation;
  clString m(method_.get());

#if 0
  TriSurfMesh *ots = new TriSurfMesh;
  osurfH_ = ots;
  if (m == "project")
  {	
    TriSurfMesh *ts = scinew TriSurfMesh;
	
    // First, set up the data point locations and values in an array.
    Array1<Point> p;

    // Get the right TriSurfMesh and grab those vals.
    if (!(ts = surfH->getTriSurfMesh()))
    {

      cerr << "Error - need a TriSurfMesh!\n";
      return;
    }
    if (ts->bcIdx.size()==0)
    {
      ts->bcIdx.resize(ts->points.size());
      ts->bcVal.resize(ts->points.size());
      for (int ii = 0; ii<ts->points.size(); ii++)
      {
	ts->bcIdx[ii] = ii;
	ts->bcVal[ii] = 0;
      }
    }
    int i;
    for (i = 0; i<ts->bcIdx.size(); i++)
    {
      p.add(ts->points[ts->bcIdx[i]]);
    }
    ColumnMatrix *mapping = scinew ColumnMatrix(ts->bcIdx.size());
    omatrixH_ = mapping;
    int *rows;
    int *cols;
    double *a;
    SparseRowMatrix *mm;
    if (potMatTCL_.get())
    {
      rows = new int[ts->bcIdx.size()];
      cols = new int[(ts->bcIdx.size()-1)];
      a = new double[(ts->bcIdx.size()-1)];
      mm = scinew SparseRowMatrix(ts->bcIdx.size()-1,
				  meshH->nodesize(),
				  rows, cols,
				  ts->bcIdx.size()-1, a);
      for (i = 0; i<ts->bcIdx.size(); i++)
      { rows[i] = i;
      }
    }
    else
    {
      rows = new int[ts->bcIdx.size()+1];
      cols = new int[ts->bcIdx.size()];
      a = new double[ts->bcIdx.size()];
      mm = scinew SparseRowMatrix(ts->bcIdx.size(),
				  meshH->nodesize(),
				  rows, cols,
				  ts->bcIdx.size(), a);
      for (i = 0; i<=ts->bcIdx.size()+1; i++)
      {
	rows[i] = i;
      }
    }
    omatH_ = mm;
    double *vals = mapping->get_rhs();
    int firstNode = 0;
    if (zeroTCL_.get())
    {
      cerr << "Skipping zero'th mesh node.\n";
      firstNode = 1;
    }
    if (m == "project")
    {
      if (p.size() > meshH->nodesize())
      {
	cerr << "Too many points to project (" << p.size() <<
	  " to " << meshH->nodesize() << ")\n";
	return;
      }

      Array1<int> selected(meshH->nodesize());
      selected.initialize(0);
      int counter = 0;
      for (int aa = 0; aa<p.size(); aa++)
      {
	double dt;
	int si = -1;
	double d;
	for (int bb = firstNode; bb<meshH->nodesize(); bb++)
	{
	  if (selected[bb]) continue;
	  dt = Vector(p[aa] - meshH->point(bb)).length2();
	  if (si==-1 || dt < d)
	  {
	    si = bb;
	    d = dt;
	  }
	}
	selected[si] = 1;

	if (!potMatTCL_.get() || aa!=0)
	{
	  a[counter] = 1;
	  cols[counter] = si;
	  counter++;
	}
	vals[aa] = si;
	ots->points.add(meshH->point(si));
      }
    }
  }
  else
  {
    cerr << "Unknown method: "<< m <<"\n";
    return;
  }
#endif

  omatrix_->send(omatrixH_);
  omat_->send(omatH_);
  osurf_->send(osurfH_);
}


} // End namespace SCIRun


