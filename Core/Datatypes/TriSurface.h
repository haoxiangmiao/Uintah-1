
/*
 *  TriSurface.h: Triangulated Surface Data type
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Datatypes_TriSurface_h
#define SCI_Datatypes_TriSurface_h 1

#include <Core/Datatypes/Surface.h>
#include <Core/Containers/Array1.h>
#include <Core/Geometry/Point.h>

namespace SCIRun {

class SurfTree;

struct TSElement {
  int i1; 
  int i2; 
  int i3;
  inline TSElement(int i1, int i2, int i3):i1(i1), i2(i2), i3(i3) {}
  inline TSElement(const TSElement& e):i1(e.i1), i2(e.i2), i3(e.i3) {}
  void* operator new(size_t);
  void operator delete(void*, size_t);
};

struct TSEdge {
  int i1;
  int i2;
  inline TSEdge(int i1, int i2):i1(i1), i2(i2) {}
  inline TSEdge(const TSEdge& e):i1(e.i1), i2(e.i2) {}
  void* operator new(size_t);
  void operator delete(void*, size_t);
};

void Pio (Piostream& stream, TSElement*& data);
void Pio (Piostream& stream, TSEdge*& data);


class SCICORESHARE TriSurface : public Surface {
  friend class SurfTree;

public:
  Array1<Point> points;
  Array1<TSElement*> elements;
  Array1<int> bcIdx;		// indices of any points w/ boundary conditions
  Array1<double> bcVal;		// the values at each boundary condition

protected:
  enum BCType {
    NodeType,
    FaceType
  };
  BCType valType;   // are the bc indices/values refering to elements or nodes

public:
  enum NormalsType {
    PointType,	// one normal per point of the surface
    VertexType,	// one normal for each vertex of each element
    ElementType, 	// one normal for each element
    NrmlsNone
  };
  NormalsType normType;

  Array1<Vector> normals;

protected:
  int haveNodeInfo;

  Array1<Array1<int> > nodeElems;	// which elements is a node part of
  Array1<Array1<int> > nodeNbrs;	// which nodes are one neighbors

private:
  int empty_index;
  int directed;	// are the triangle all ordered clockwise?
  double distance(const Point &p, int i, int *type, Point *pp=0);
  int find_or_add(const Point &p);

public:
  TriSurface();
  TriSurface(const TriSurface& copy);
  virtual ~TriSurface();
  virtual Surface *clone();

  // Persistent representation.
  virtual void io(Piostream&);
  static PersistentTypeID type_id;

  // Virtual surface interface.
  virtual bool inside(const Point& p);
  virtual void construct_grid();
  virtual void construct_grid(int, int, int, const Point &, double);
  virtual void construct_hash(int, int, const Point &, double);
  virtual GeomObj* get_geom(const ColorMapHandle&);

  void buildNormals(NormalsType);
  void buildNodeInfo();

  SurfTree *toSurfTree();


  // these two were implemented for isosurfacing btwn two surfaces
  // (MorphMesher3d module/class)
  int cautious_add_triangle(const Point &p1, const Point &p2, const Point &p3,
			    int cw=0);
  int get_closest_vertex_id(const Point &p1, const Point &p2,
			    const Point &p3);


  int intersect(const Point& origin, const Vector& dir, double &d, int &v, int face);

  int add_triangle(int i1, int i2, int i3, int cw=0);


protected:
  // pass in allocated surfaces for conn and d_conn. NOTE: contents will be
  // overwritten
  void separate(int idx, TriSurface* conn, TriSurface* d_conn, int updateConnIndices=1, int updateDConnIndices=1);

  // NOTE: if elements have been added or removed from the surface
  // remove_empty_index() MUST be called before passing a TriSurface
  // to another module!  
  void remove_empty_index();
  void order_faces();
  inline int is_directed() {return directed;}
  void add_point(const Point& p);
  void remove_triangle(int i);
  double distance(const Point &p, Array1<int> &res, Point *pp=0);
    
  // this is for random distributions on the surface...

  Array1< Point > samples; // random points
  Array1< double > weights; // weight for particular element

  void compute_samples(int nsamp); // compute the "weights" and 

  void distribute_samples(); // samples are really computed
};

} // End namespace SCIRun

#endif /* SCI_Datatytpes_TriSurface_h */
