//  PointCloudGeom.h - A group of points in 3 space
//
//  Written by:
//   Eric Kuehne
//   Department of Computer Science
//   University of Utah
//   April 2000
//
//  Copyright (C) 2000 SCI Institute


#ifndef SCI_project_PointCloudGeom_h
#define SCI_project_PointCloudGeom_h 1

#include <SCICore/Geometry/Vector.h>
#include <SCICore/Geometry/Point.h>
#include <SCICore/Geom/GeomTriangles.h>
#include <SCICore/Geom/GeomPolyline.h>
#include <SCICore/Datatypes/UnstructuredGeom.h>
#include <SCICore/Containers/LockingHandle.h>
#include <SCICore/Math/MiscMath.h>
#include <SCICore/Util/DebugStream.h>
#include <sstream>
#include <vector>
#include <string>
#include <set>


namespace SCICore{
namespace Datatypes{

using SCICore::Geometry::Vector;
using SCICore::Geometry::Point;
using SCICore::Geometry::Min;
using SCICore::Geometry::Max;
using SCICore::GeomSpace::GeomTrianglesP;
using SCICore::GeomSpace::GeomPolyline;
using std::vector;
using std::string;
using SCICore::PersistentSpace::Piostream;
using SCICore::PersistentSpace::PersistentTypeID;
using SCICore::Math::Interpolate;
using SCICore::Util::DebugStream;

class PointCloudGeom:public UnstructuredGeom
{
public:

  PointCloudGeom(const vector<NodeSimp>&);
  ~PointCloudGeom();

  virtual string get_info();
  
  //////////
  // Interpolate
  template <class A>
  int slinterpolate(A* att, elem_t, const Point& p, double& outval,
		    double eps=1.0e-6);

  //////////
  // set nodes and tets vectors
  // deletes these pointers if they are allready set
  void set_nodes(const vector<NodeSimp>&);

 ///////////
  // Persistent representation...
  virtual void io(Piostream&);
  static PersistentTypeID type_id;

protected:

  virtual bool compute_bbox();


  vector<NodeSimp> nodes;

private:
  static DebugStream dbg;
};

template <class A>
int PointCloudGeom::slinterpolate(A* att, elem_t elem_type, const Point& p, double& outval,
				  double eps){

}

} // end Datatypes
} // end SCICore


#endif
