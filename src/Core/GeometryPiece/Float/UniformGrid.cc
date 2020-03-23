/*
 * The MIT License
 *
 * Copyright (c) 1997-2018 The University of Utah
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

#include <Core/GeometryPiece/UniformGrid.h>
#include <Core/Malloc/Allocator.h>
#include <iostream>
#include <map>

using namespace Uintah;
using namespace std;


Tri::Tri(FloatPoint& p1, FloatPoint& p2, FloatPoint& p3)
{
  d_points[0] = p1;
  d_points[1] = p2;
  d_points[2] = p3;
  d_plane = FloatPlane(p1, p2, p3);
}

Tri::Tri()
{
}

Tri::~Tri()
{
}

FloatPoint Tri::centroid()
{
  FloatVector cent(0.,0.,0);
  for (int i = 0; i < 3; i++)
    cent += d_points[i].asVector();

  cent /= 3.;

  return FloatPoint(cent.x(),cent.y(),cent.z());
}

FloatPoint Tri::vertex(int i)
{
  return d_points[i];
}

list<Tri> Tri::makeTriList(vector<IntVector>& tris ,vector<FloatPoint>& pts)
{
  list<Tri> tri_list;
  vector<IntVector>::const_iterator tri_itr;
  for (tri_itr = tris.begin(); tri_itr != tris.end(); ++tri_itr) {
    Tri triangle = Tri(pts[(*tri_itr).x()],pts[(*tri_itr).y()],
		       pts[(*tri_itr).z()]);
    tri_list.push_back(triangle);
  }
  return tri_list;
}

bool Tri::inside(Point& nfpt)
{
  FloatPoint pt(nfpt.x(), nfpt.y(), nfpt.z());
  FloatVector plane_normal = d_plane.normal();
  FloatVector plane_normal_abs = Abs(plane_normal);
  double largest = plane_normal_abs.maxComponent();
  // WARNING: if dominant_coord is not 1-3, then this code breaks...
  int dominant_coord = -1;
  if (largest == plane_normal_abs.x()) dominant_coord = 1;
  else if (largest == plane_normal_abs.y()) dominant_coord = 2;
  else if (largest == plane_normal_abs.z()) dominant_coord = 3;

  coord x = X, y = Y; // Initialization is to remove compiler warning.
  switch (dominant_coord)
    {
    case 1:
      x = Y;
      y = Z;
      break;
    case 2:
      x = Z;
      y = X;
      break;
    case 3:
      x = X;
      y = Y;
      break;
    }

  double tx = pt(x), ty = pt(y);

  FloatPoint *p1 = &d_points[2], *p2 = d_points;
  int yflag0 = ((*p1)(y) >= ty);

  bool inside = false;

  for (int i = 3; i--;) {
    int yflag1 = ((*p2)(y) >= ty);
    if (yflag0 != yflag1) {
      int xflag0 = ((*p1)(x) >= tx);
      if (xflag0 == ((*p2)(x) >= tx)) {
	if (xflag0)
	  inside = !inside;
      } else {
	double cmp = ((*p2)(x)-((*p2)(y)-ty)*((*p1)(x)-(*p2)(x))/((*p1)(y)-(*p2)(y)));
	//	if (cmp >= tx)
	//	if (abs(cmp - tx) >= 0.)
	if (cmp - tx >= -1.e-8)
	  inside = !inside;
      }
    }
    yflag0 = yflag1;
    p1 = p2;
    p2++;
  }
  return bool(inside);
}

FloatPlane Tri::plane()
{
  return d_plane;
}

UniformGrid::UniformGrid(Box& bound_box)
{
  const IntVector low(0,0,0), hi(10,10,10);
  Vector diff = Vector(hi.x(),hi.y(),hi.z()) - Vector(low.x(),low.y(),low.z());
  d_bound_box = bound_box;
  Vector dmm = (bound_box.upper().asVector()-bound_box.lower().asVector())/diff;
  d_max_min = FloatVector(dmm.x(), dmm.y(), dmm.z());
  d_grid.resize(low,hi);
}

UniformGrid::UniformGrid(const UniformGrid& copy)
{
  d_bound_box = copy.d_bound_box;
  d_max_min = copy.d_max_min;

  d_grid.resize(copy.d_grid.getLowIndex(),copy.d_grid.getHighIndex());
  parallel_for( d_grid.range(), [&](int i, int j, int k) {
    d_grid(i,j,k) = copy.d_grid(i,j,k);
  });

}


UniformGrid& UniformGrid::operator=(const UniformGrid& rhs)
{
  if (this == &rhs)
    return *this;

  parallel_for( d_grid.range(), [&](int i, int j, int k) {
    d_grid(i,j,k).clear();
  });

  d_grid.resize(rhs.d_grid.getLowIndex(),rhs.d_grid.getHighIndex());

  d_bound_box = rhs.d_bound_box;
  d_max_min = rhs.d_max_min;

  parallel_for( d_grid.range(), [&](int i, int j, int k) {
    d_grid(i,j,k) = rhs.d_grid(i,j,k);
  });

  return *this;
}

UniformGrid::~UniformGrid()
{
}

IntVector UniformGrid::cellID(FloatPoint point)
{
  Point pt(point.x(), point.y(), point.z());
  Vector pt_diff = pt.asVector() - (d_bound_box.lower()).asVector();
  Vector dmm = Vector(d_max_min.x(), d_max_min.y(), d_max_min.z());
  Vector id = pt_diff/dmm;
  int i = (int)floor(id.x());
  int j = (int)floor(id.y());
  int k = (int)floor(id.z());
  return IntVector(i,j,k);
}

void UniformGrid::buildUniformGrid(list<Tri>& polygons)
{
  for (list<Tri>::iterator tri = polygons.begin(); tri != polygons.end();
       tri++) {
    IntVector v0 = cellID(tri->vertex(0));
    IntVector v1 = cellID(tri->vertex(1));
    IntVector v2 = cellID(tri->vertex(2));
#if 0
    if (v0 > d_grid.getHighIndex())
      cout << "v0 = " << v0 << endl;
    if (v1 > d_grid.getHighIndex())
      cout << "v1 = " << v1 << endl;
    if (v2 > d_grid.getHighIndex())
      cout << "v2 = " << v2 << endl;

    cout << "Tri = " << tri->vertex(0) << " " << tri->vertex(1) << " "
	 << tri->vertex(2) << endl;
    cout << "v0 " << v0 << " v1 " << v1 << " v2 " << v2 << endl;
#endif
    IntVector low = Min(v0,v1);
    low = Min(low,v2);
    IntVector hi = Max(v0,v1);
    hi = Max(hi,v2);
    for (int i = low.x(); i <= hi.x(); i++)
      for (int j = low.y(); j <= hi.y(); j++)
	for (int k = low.z(); k <= hi.z(); k++) {
	  IntVector id(i,j,k);
	  //  cout << "Inserting into cellID = " << id << endl;
	  d_grid[id].push_back(*tri);
	}
  }
}

void UniformGrid::countIntersections(const Point& pt, int& crossings)
{
  // Make a ray and shoot it in the +x direction

  FloatPoint fpt = FloatPoint(pt.x(), pt.y(), pt.z());
  FloatVector infinity = FloatVector(pt.x()+1e10,pt.y(),pt.z());
  IntVector test_pt_id = cellID(fpt);
  IntVector start = d_grid.getLowIndex();
  IntVector stop = d_grid.getHighIndex();

  map<double,Tri> cross_map;

  for (int i = start.x(); i < stop.x(); i++) {

    IntVector curr(i,test_pt_id.y(),test_pt_id.z());
    list<Tri> tris = d_grid[curr];
    for (list<Tri>::iterator itr = tris.begin(); itr != tris.end(); ++itr) {
      FloatPoint fhit;
      Point hit;

      if ((itr->plane()).Intersect(fpt,infinity,fhit)) {
        hit=Point(fhit.x(), fhit.y(), fhit.z());
	FloatVector int_ray = fhit.asVector() - fpt.asVector();
	double cos_angle = Dot(infinity,int_ray)/
	  (infinity.length()*int_ray.length());
	if (cos_angle < 0.)
	  continue;
	if (itr->inside(hit)) {
#if 0
	  cout << "Inside_new hit = " << hit << "vertices: "
	       << itr->vertex(0) << " " << itr->vertex(1) <<  " "
	       << itr->vertex(2) << endl;
#endif
	  double distance = int_ray.length();
	  map<double,Tri>::const_iterator duplicate = cross_map.find(distance);
	  if (duplicate == cross_map.end()) {
	    cross_map[distance] = *itr;
	    crossings++;
	  }
	}
      }
    }
  }
}
