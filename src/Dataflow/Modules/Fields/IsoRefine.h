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


//    File   : ClipField.h
//    Author : Michael Callahan
//    Date   : August 2001

#if !defined(ClipField_h)
#define ClipField_h

#include <Dataflow/Network/Module.h>
#include <Core/Util/TypeDescription.h>
#include <Core/Util/DynamicLoader.h>
#include <Core/Util/ProgressReporter.h>
#include <Core/Datatypes/SparseRowMatrix.h>
#include <Core/Basis/QuadBilinearLgn.h>
#include <Core/Basis/HexTrilinearLgn.h>
#include <Core/Basis/TriLinearLgn.h>
#include <Core/Datatypes/QuadSurfMesh.h>
#include <Core/Datatypes/HexVolMesh.h>
#include <Core/Datatypes/GenericField.h>

#include <sci_hash_map.h>
#include <algorithm>
#include <set>

#include <Dataflow/Modules/Fields/share.h>

namespace SCIRun {
typedef QuadSurfMesh<QuadBilinearLgn<Point> > QSMesh;
typedef HexVolMesh<HexTrilinearLgn<Point> > HVMesh;
class GuiInterface;

class SCISHARE IsoRefineAlgo : public DynamicAlgoBase
{
public:

  virtual FieldHandle execute(ProgressReporter *reporter, FieldHandle fieldh,
			      double isoval, bool lte,
			      MatrixHandle &interpolant) = 0;

  //! support the dynamically compiled algorithm concept
  static CompileInfoHandle get_compile_info(const TypeDescription *fsrc,
					    string ext);

  static int hex_reorder_table[14][8];
};


template <class FIELD>
class IsoRefineAlgoQuad : public IsoRefineAlgo
{
public:
  //! virtual interface. 
  virtual FieldHandle execute(ProgressReporter *reporter, FieldHandle fieldh,
			      double isoval, bool lte,
			      MatrixHandle &interpolant);

  struct edgepair_t
  {
    unsigned int first;
    unsigned int second;
  };

  struct edgepairequal
  {
    bool operator()(const edgepair_t &a, const edgepair_t &b) const
    {
      return a.first == b.first && a.second == b.second;
    }
  };

  struct edgepairless
  {
    bool operator()(const edgepair_t &a, const edgepair_t &b)
    {
      return less(a, b);
    }
    static bool less(const edgepair_t &a, const edgepair_t &b)
    {
      return a.first < b.first || a.first == b.first && a.second < b.second;
    }
  };

#ifdef HAVE_HASH_MAP
  struct edgepairhash
  {
    unsigned int operator()(const edgepair_t &a) const
    {
#if defined(__ECC) || defined(_MSC_VER)
      hash_compare<unsigned int> h;
#else
      hash<unsigned int> h;
#endif
      return h(a.first ^ a.second);
    }
#if defined(__ECC) || defined(_MSC_VER)

      // These are particularly needed by ICC's hash stuff
      static const size_t bucket_size = 4;
      static const size_t min_buckets = 8;
      
      // This is a less than function.
      bool operator()(const edgepair_t & a, const edgepair_t & b) const {
        return edgepairless::less(a,b);
      }
#endif // endif ifdef __ICC
  };
#endif

#ifdef HAVE_HASH_MAP
#  if defined(__ECC) || defined(_MSC_VER)
  typedef hash_map<edgepair_t,
		   QSMesh::Node::index_type,
		   edgepairhash> edge_hash_type;
#  else
  typedef hash_map<edgepair_t,
		   QSMesh::Node::index_type,
		   edgepairhash,
		   edgepairequal> edge_hash_type;
#  endif
#else
  typedef map<edgepair_t,
	      QSMesh::Node::index_type,
	      edgepairless> edge_hash_type;
#endif

  typename QSMesh::Node::index_type
  lookup(typename FIELD::mesh_type *mesh,
         QSMesh *refined,
         edge_hash_type &edgemap,
         typename FIELD::mesh_type::Node::index_type a,
         typename FIELD::mesh_type::Node::index_type b)
  {
    edgepair_t ep;
    ep.first = a; ep.second = b;
    const typename edge_hash_type::iterator loc = edgemap.find(ep);
    if (loc == edgemap.end())
    {
      Point pa, pb;
      mesh->get_point(pa, a);
      mesh->get_point(pb, b);
      const Point inbetween = Interpolate(pa, pb, 1.0/3.0);
      const QSMesh::Node::index_type newnode = refined->add_point(inbetween);
      edgemap[ep] = newnode;
      return newnode;
    }
    else
    {
      return (*loc).second;
    }
  }
};


template <class FIELD>
FieldHandle
IsoRefineAlgoQuad<FIELD>::execute(ProgressReporter *reporter,
                                  FieldHandle fieldh,
                                  double isoval, bool lte,
                                  MatrixHandle &interp)
{
  FIELD *field = dynamic_cast<FIELD*>(fieldh.get_rep());
  typename FIELD::mesh_type *mesh =
      dynamic_cast<typename FIELD::mesh_type *>(fieldh->mesh().get_rep());
  QSMesh *refined = scinew QSMesh();
  refined->copy_properties(mesh);

  edge_hash_type emap;

  typename FIELD::mesh_type::Node::array_type onodes(4);
  QSMesh::Node::array_type oqnodes(4);
  QSMesh::Node::array_type nnodes(4);
  typename FIELD::value_type v[4];
  Point p[4];
  
  // Copy all of the nodes from mesh to refined.  They won't change,
  // we only add nodes.
  typename FIELD::mesh_type::Node::iterator bni, eni;
  mesh->begin(bni); mesh->end(eni);
  while (bni != eni)
  {
    mesh->get_point(p[0], *bni);
    refined->add_point(p[0]);
    ++bni;
  }

  typename FIELD::mesh_type::Elem::iterator bi, ei;
  mesh->begin(bi); mesh->end(ei);
  while (bi != ei)
  {
    mesh->get_nodes(onodes, *bi);
    
    // Get the values and compute an inside/outside mask.
    unsigned int inside = 0;
    for (unsigned int i = 0; i < onodes.size(); i++)
    {
      mesh->get_center(p[i], onodes[i]);
      field->value(v[i], onodes[i]);
      oqnodes[i] = QSMesh::Node::index_type((unsigned int)onodes[i]);
      inside = inside << 1;
      if (v[i] > isoval)
      {
        inside |= 1;
      }
    }

    bool refine_elem = false;
    
    // Invert the mask if we are doing less than.
    if (lte) { inside = ~inside & 0xf; }
    
    if (!refine_elem && inside == 0)
    {
      // Nodes are the same order, so just add the element.
      refined->add_elem(oqnodes);
    }
    else if (!refine_elem &&
             (inside == 1 || inside == 2 || inside == 4 || inside == 8))
    {
      int index;
      if (inside == 1) index = 3;
      else if (inside == 2) index = 2;
      else if (inside == 4) index = 1;
      else index = 0;

      const int i0 = index;
      const int i1 = (index+1)%4;
      const int i2 = (index+2)%4;
      const int i3 = (index+3)%4;

      const Point interior = Interpolate(p[i0], p[i2], 1.0/3.0);

      const QSMesh::Node::index_type interior_node =
        refined->add_point(interior);

      nnodes[0] = oqnodes[i0];
      nnodes[1] = lookup(mesh, refined, emap, onodes[i0], onodes[i1]);
      nnodes[2] = interior_node;
      nnodes[3] = lookup(mesh, refined, emap, onodes[i0], onodes[i3]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, emap, onodes[i0], onodes[i1]);
      nnodes[1] = oqnodes[i1];
      nnodes[2] = oqnodes[i2];
      nnodes[3] = interior_node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, emap, onodes[i0], onodes[i3]);
      nnodes[1] = interior_node;
      nnodes[2] = oqnodes[i2];
      nnodes[3] = oqnodes[i3];
      refined->add_elem(nnodes);
    }
    else if (!refine_elem && (inside == 5 || inside == 10))
    {
      int index = 0;
      if (inside == 5) index = 1;

      const int i0 = index;
      const int i1 = (index+1)%4;
      const int i2 = (index+2)%4;
      const int i3 = (index+3)%4;

      const Point center = Interpolate(p[index], p[(index+2)%4], 1.0/2.0);
      const QSMesh::Node::index_type center_node = refined->add_point(center);

      nnodes[0] = oqnodes[i0];
      nnodes[1] = lookup(mesh, refined, emap, onodes[i0], onodes[i1]);
      nnodes[2] = center_node;
      nnodes[3] = lookup(mesh, refined, emap, onodes[i0], onodes[i3]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, emap, onodes[i0], onodes[i1]);
      nnodes[1] = oqnodes[i1];
      nnodes[2] = lookup(mesh, refined, emap, onodes[i2], onodes[i1]);
      nnodes[3] = center_node;
      refined->add_elem(nnodes);

      nnodes[0] = center_node;
      nnodes[1] = lookup(mesh, refined, emap, onodes[i2], onodes[i1]);
      nnodes[2] = oqnodes[i2];
      nnodes[3] = lookup(mesh, refined, emap, onodes[i2], onodes[i3]);
      refined->add_elem(nnodes);
      
      nnodes[0] = lookup(mesh, refined, emap, onodes[i0], onodes[i3]);
      nnodes[1] = center_node;
      nnodes[2] = lookup(mesh, refined, emap, onodes[i2], onodes[i3]);
      nnodes[3] = oqnodes[i3];
      refined->add_elem(nnodes);
    }
    else
    {
      Point interiorp[4];
      QSMesh::Node::array_type inodes(4);
      for (unsigned int i = 0; i < 4; i++)
      {
        interiorp[i] = Interpolate(p[i], p[(i+2)%4], 1.0/3.0);
        inodes[i] = refined->add_point(interiorp[i]);
      }
      refined->add_elem(inodes);
      
      for (unsigned int i = 0; i < 4; i++)
      {
        if (inside & (1 << (3-i)))
        {
          nnodes[0] = oqnodes[i];
          nnodes[1] = lookup(mesh, refined, emap, onodes[i], onodes[(i+1)%4]);
          nnodes[2] = inodes[i];
          nnodes[3] = lookup(mesh, refined, emap, onodes[i], onodes[(i+3)%4]);
          refined->add_elem(nnodes);
        }

        if (inside & (1 << (3-i)))
        {
          nnodes[0] = lookup(mesh, refined, emap, onodes[i], onodes[(i+1)%4]);
        }
        else
        {
          nnodes[0] = oqnodes[i];
        }
        if (inside & (1 << (3 - (i+1)%4)))
        {
          nnodes[1] = lookup(mesh, refined, emap, onodes[(i+1)%4], onodes[i]);
        }
        else
        {
          nnodes[1] = oqnodes[(i+1)%4];
        }
        nnodes[2] = inodes[(i+1)%4];
        nnodes[3] = inodes[i];
        refined->add_elem(nnodes);
      }
    }
    ++bi;
  }

  GenericField<QSMesh, QuadBilinearLgn<double>, vector<double> > *ofield =
    scinew GenericField<QSMesh, QuadBilinearLgn<double>, vector<double> >(refined);
  ofield->copy_properties(fieldh.get_rep());
  return ofield;
}



template <class FIELD>
class IsoRefineAlgoHex : public IsoRefineAlgo
{
public:
  //! virtual interface. 
  virtual FieldHandle execute(ProgressReporter *reporter, FieldHandle fieldh,
			      double isoval, bool lte,
			      MatrixHandle &interpolant);


  struct edgepair_t
  {
    unsigned int first;
    unsigned int second;
  };

  struct edgepairequal
  {
    bool operator()(const edgepair_t &a, const edgepair_t &b) const
    {
      return a.first == b.first && a.second == b.second;
    }
  };

  struct edgepairless
  {
    bool operator()(const edgepair_t &a, const edgepair_t &b)
    {
      return less(a, b);
    }
    static bool less(const edgepair_t &a, const edgepair_t &b)
    {
      return a.first < b.first || a.first == b.first && a.second < b.second;
    }
  };

#ifdef HAVE_HASH_MAP
  struct edgepairhash
  {
    unsigned int operator()(const edgepair_t &a) const
    {
#if defined(__ECC) || defined(_MSC_VER)
      hash_compare<unsigned int> h;
#else
      hash<unsigned int> h;
#endif
      return h(a.first ^ a.second);
    }
#if defined(__ECC) || defined(_MSC_VER)

      // These are particularly needed by ICC's hash stuff
      static const size_t bucket_size = 4;
      static const size_t min_buckets = 8;
      
      // This is a less than function.
      bool operator()(const edgepair_t & a, const edgepair_t & b) const {
        return edgepairless::less(a,b);
      }
#endif // endif ifdef __ICC
  };
#endif

#ifdef HAVE_HASH_MAP
#  if defined(__ECC) || defined(_MSC_VER)
  typedef hash_map<edgepair_t,
		   HVMesh::Node::index_type,
		   edgepairhash> edge_hash_type;
#  else
  typedef hash_map<edgepair_t,
		   HVMesh::Node::index_type,
		   edgepairhash,
		   edgepairequal> edge_hash_type;
#  endif
#else
  typedef map<edgepair_t,
	      HVMesh::Node::index_type,
	      edgepairless> edge_hash_type;
#endif

  HVMesh::Node::index_type
  lookup(typename FIELD::mesh_type *mesh,
         HVMesh *refined,
         edge_hash_type &edgemap,
         typename FIELD::mesh_type::Node::index_type a,
         typename FIELD::mesh_type::Node::index_type b)
  {
    edgepair_t ep;
    ep.first = a; ep.second = b;
    const typename edge_hash_type::iterator loc = edgemap.find(ep);
    if (loc == edgemap.end())
    {
      Point pa, pb;
      mesh->get_point(pa, a);
      mesh->get_point(pb, b);
      const Point inbetween = Interpolate(pa, pb, 1.0/3.0);
      const HVMesh::Node::index_type newnode = refined->add_point(inbetween);
      edgemap[ep] = newnode;
      return newnode;
    }
    else
    {
      return (*loc).second;
    }
  }

  int pattern_table[256][2];

  inline unsigned int iedge(unsigned int a, unsigned int b)
  {
    return (1<<(7-a)) | (1<<(7-b));
  }

  inline unsigned int iface(unsigned int a, unsigned int b,
                     unsigned int c, unsigned int d)
  {
    return iedge(a, b) | iedge(c, d);
  }

  inline void set_table(int i, int pattern, int reorder)
  {
    pattern_table[i][0] = pattern;
    pattern_table[i][1] = reorder;
  }

  void init_pattern_table()
  {
    for (int i = 0; i < 256; i++)
    {
      set_table(i, -1, 0);
    }

    set_table(0, 0, 0);

    // Add corners
    set_table(1, 1, 7);
    set_table(2, 1, 6);
    set_table(4, 1, 5);
    set_table(8, 1, 4);
    set_table(16, 1, 3);
    set_table(32, 1, 2);
    set_table(64, 1, 1);
    set_table(128, 1, 0);

    // Add edges
    set_table(iedge(0, 1), 2, 0);
    set_table(iedge(1, 2), 2, 1);
    set_table(iedge(2, 3), 2, 2);
    set_table(iedge(3, 0), 2, 3);
    set_table(iedge(4, 5), 2, 5);
    set_table(iedge(5, 6), 2, 6);
    set_table(iedge(6, 7), 2, 7);
    set_table(iedge(7, 4), 2, 4);
    set_table(iedge(0, 4), 2, 8);
    set_table(iedge(1, 5), 2, 9);
    set_table(iedge(2, 6), 2, 10);
    set_table(iedge(3, 7), 2, 11);

    set_table(iface(0, 1, 2, 3), 4, 0);
    set_table(iface(0, 1, 5, 4), 4, 12);
    set_table(iface(1, 2, 6, 5), 4, 9);
    set_table(iface(2, 3, 7, 6), 4, 13);
    set_table(iface(3, 0, 4, 7), 4, 8);
    set_table(iface(4, 5, 6, 7), 4, 7);

    set_table(255, 8, 0);
  }
};


template <class FIELD>
FieldHandle
IsoRefineAlgoHex<FIELD>::execute(ProgressReporter *reporter,
                                 FieldHandle fieldh,
                                 double isoval, bool lte,
                                 MatrixHandle &interp)
{
  FIELD *field = dynamic_cast<FIELD*>(fieldh.get_rep());
  typename FIELD::mesh_type *mesh =
      dynamic_cast<typename FIELD::mesh_type *>(fieldh->mesh().get_rep());
  HVMesh *refined = scinew HVMesh();
  refined->copy_properties(mesh);

  init_pattern_table();
  edge_hash_type edgemap;
  
  typename FIELD::mesh_type::Node::array_type onodes(8);
  HVMesh::Node::array_type ohnodes(8);
  HVMesh::Node::array_type nnodes(8);
  //typename FIELD::mesh_type::Node::array_type inodes(8);
  typename FIELD::value_type v[8];
  Point p[8];
  
  // Copy all of the nodes from mesh to refined.  They won't change,
  // we only add nodes.
  typename FIELD::mesh_type::Node::iterator bni, eni;
  mesh->begin(bni); mesh->end(eni);
  while (bni != eni)
  {
    mesh->get_point(p[0], *bni);
    refined->add_point(p[0]);
    ++bni;
  }

  typename FIELD::mesh_type::Elem::iterator bi, ei;
  mesh->begin(bi); mesh->end(ei);
  while (bi != ei)
  {
    mesh->get_nodes(onodes, *bi);
    
    // Get the values and compute an inside/outside mask.
    unsigned int inside = 0;
    unsigned int inside_count = 0;
    for (unsigned int i = 0; i < onodes.size(); i++)
    {
      mesh->get_center(p[i], onodes[i]);
      field->value(v[i], onodes[i]);
      ohnodes[i] = HVMesh::Node::index_type((unsigned int)onodes[i]);
      inside = inside << 1;
      if (v[i] > isoval)
      {
        inside |= 1;
        inside_count++;
      }
    }

    // Invert the mask if we are doing less than.
    if (lte) { inside = ~inside & 0xff; inside_count = 8 - inside_count; }

    const int pattern = pattern_table[inside][0];
    const int which = pattern_table[inside][1];

    if (pattern == 0)
    {
      // Nodes are the same order, so just add the element.
      refined->add_elem(ohnodes);
    }
    else if (pattern == 1)
    {
      const int *ro = hex_reorder_table[which];
      
      const Point i06 = Interpolate(p[ro[0]], p[ro[6]], 1.0/3.0);
      const HVMesh::Node::index_type i06node = refined->add_point(i06);

      // Add this corner.
      nnodes[0] = ohnodes[ro[0]];
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[6] = i06node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      // Add the other three pieces.
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[1] = ohnodes[ro[1]];
      nnodes[2] = ohnodes[ro[2]];
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = i06node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[2] = ohnodes[ro[2]];
      nnodes[3] = ohnodes[ro[3]];
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[5] = i06node;
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);
      
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[2] = i06node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);
    }
    else if (pattern == 2)
    {
      const int *ro = hex_reorder_table[which];

      const Point i06 = Interpolate(p[ro[0]], p[ro[6]], 1.0/3.0);
      const Point i17 = Interpolate(p[ro[1]], p[ro[7]], 1.0/3.0);
      const Point i60 = Interpolate(p[ro[6]], p[ro[0]], 1.0/3.0);
      const Point i71 = Interpolate(p[ro[7]], p[ro[1]], 1.0/3.0);
      const HVMesh::Node::index_type i06node = refined->add_point(i06);
      const HVMesh::Node::index_type i17node = refined->add_point(i17);
      const HVMesh::Node::index_type i60node = refined->add_point(i60);
      const HVMesh::Node::index_type i71node = refined->add_point(i71);

      // Leading edge.
      nnodes[0] = ohnodes[ro[0]];
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[6] = i06node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[0]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[6] = i17node;
      nnodes[7] = i06node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[0]]);
      nnodes[1] = ohnodes[ro[1]];
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[7] = i17node;
      refined->add_elem(nnodes);

      // Top center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[3] = ohnodes[ro[3]];
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[5] = i06node;
      nnodes[6] = i71node;
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[4] = i06node;
      nnodes[5] = i17node;
      nnodes[6] = i60node;
      nnodes[7] = i71node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[2]]);
      nnodes[2] = ohnodes[ro[2]];
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[4] = i17node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = i60node;
      refined->add_elem(nnodes);

      // Front Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[2] = i06node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[6] = i71node;
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[2] = i17node;
      nnodes[3] = i06node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[6] = i60node;
      nnodes[7] = i71node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[5]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[3] = i17node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = i60node;
      refined->add_elem(nnodes);

      // Outside wedges
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[2] = ohnodes[ro[2]];
      nnodes[3] = ohnodes[ro[3]];
      nnodes[4] = i71node;
      nnodes[5] = i60node;
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[2] = i60node;
      nnodes[3] = i71node;
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);
    }
    else if (pattern == 4)
    {
      const int *ro = hex_reorder_table[which];

      // Interior
      const Point i06 = Interpolate(p[ro[0]], p[ro[6]], 1.0/3.0);
      const Point i17 = Interpolate(p[ro[1]], p[ro[7]], 1.0/3.0);
      const Point i24 = Interpolate(p[ro[2]], p[ro[4]], 1.0/3.0);
      const Point i35 = Interpolate(p[ro[3]], p[ro[5]], 1.0/3.0);
      const Point i42a = Interpolate(p[ro[4]], p[ro[2]], 1.0/3.0);
      const Point i53a = Interpolate(p[ro[5]], p[ro[3]], 1.0/3.0);
      const Point i60a = Interpolate(p[ro[6]], p[ro[0]], 1.0/3.0);
      const Point i71a = Interpolate(p[ro[7]], p[ro[1]], 1.0/3.0);
      const Point i42 = Interpolate(i06, i42a, 0.5);
      const Point i53 = Interpolate(i17, i53a, 0.5);
      const Point i60 = Interpolate(i24, i60a, 0.5);
      const Point i71 = Interpolate(i35, i71a, 0.5);
      const HVMesh::Node::index_type i06node = refined->add_point(i06);
      const HVMesh::Node::index_type i17node = refined->add_point(i17);
      const HVMesh::Node::index_type i24node = refined->add_point(i24);
      const HVMesh::Node::index_type i35node = refined->add_point(i35);
      const HVMesh::Node::index_type i42node = refined->add_point(i42);
      const HVMesh::Node::index_type i53node = refined->add_point(i53);
      const HVMesh::Node::index_type i60node = refined->add_point(i60);
      const HVMesh::Node::index_type i71node = refined->add_point(i71);

      // Top Front
      nnodes[0] = ohnodes[ro[0]];
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[6] = i06node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[0]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[6] = i17node;
      nnodes[7] = i06node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[0]]);
      nnodes[1] = ohnodes[ro[1]];
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[7] = i17node;
      refined->add_elem(nnodes);

      // Top Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[0]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[5] = i06node;
      nnodes[6] = i35node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[4] = i06node;
      nnodes[5] = i17node;
      nnodes[6] = i24node;
      nnodes[7] = i35node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[1]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[4] = i17node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[7] = i24node;
      refined->add_elem(nnodes);

      // Top Back
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[0]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[2]]);
      nnodes[3] = ohnodes[ro[3]];
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      nnodes[5] = i35node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[2]]);
      nnodes[4] = i35node;
      nnodes[5] = i24node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[1]]);
      nnodes[2] = ohnodes[ro[2]];
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[3]]);
      nnodes[4] = i24node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[6]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      // Front
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[2] = i06node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[6] = i42node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[2] = i17node;
      nnodes[3] = i06node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[6] = i53node;
      nnodes[7] = i42node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[5]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[3] = i17node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[7] = i53node;
      refined->add_elem(nnodes);

      // Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[1] = i06node;
      nnodes[2] = i35node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      nnodes[5] = i42node;
      nnodes[6] = i71node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      refined->add_elem(nnodes);

      nnodes[0] = i06node;
      nnodes[1] = i17node;
      nnodes[2] = i24node;
      nnodes[3] = i35node;
      nnodes[4] = i42node;
      nnodes[5] = i53node;
      nnodes[6] = i60node;
      nnodes[7] = i71node;
      refined->add_elem(nnodes);

      nnodes[0] = i17node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[3] = i24node;
      nnodes[4] = i53node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[7] = i60node;
      refined->add_elem(nnodes);

      // Back
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      nnodes[1] = i35node;
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[7]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      nnodes[5] = i71node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);

      nnodes[0] = i35node;
      nnodes[1] = i24node;
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      nnodes[4] = i71node;
      nnodes[5] = i60node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      refined->add_elem(nnodes);

      nnodes[0] = i24node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[6]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      nnodes[4] = i60node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      refined->add_elem(nnodes);

      // Bottom Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      nnodes[1] = i42node;
      nnodes[2] = i71node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);

      nnodes[0] = i42node;
      nnodes[1] = i53node;
      nnodes[2] = i60node;
      nnodes[3] = i71node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      refined->add_elem(nnodes);

      nnodes[0] = i53node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[3] = i60node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      refined->add_elem(nnodes);

      // Bottom
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);
    }
    else if (pattern == 8)
    {
      const int *ro = hex_reorder_table[which];

      // Interior
      const Point i06 = Interpolate(p[ro[0]], p[ro[6]], 1.0/3.0);
      const Point i17 = Interpolate(p[ro[1]], p[ro[7]], 1.0/3.0);
      const Point i24 = Interpolate(p[ro[2]], p[ro[4]], 1.0/3.0);
      const Point i35 = Interpolate(p[ro[3]], p[ro[5]], 1.0/3.0);
      const Point i42 = Interpolate(p[ro[4]], p[ro[2]], 1.0/3.0);
      const Point i53 = Interpolate(p[ro[5]], p[ro[3]], 1.0/3.0);
      const Point i60 = Interpolate(p[ro[6]], p[ro[0]], 1.0/3.0);
      const Point i71 = Interpolate(p[ro[7]], p[ro[1]], 1.0/3.0);
      const HVMesh::Node::index_type i06node = refined->add_point(i06);
      const HVMesh::Node::index_type i17node = refined->add_point(i17);
      const HVMesh::Node::index_type i24node = refined->add_point(i24);
      const HVMesh::Node::index_type i35node = refined->add_point(i35);
      const HVMesh::Node::index_type i42node = refined->add_point(i42);
      const HVMesh::Node::index_type i53node = refined->add_point(i53);
      const HVMesh::Node::index_type i60node = refined->add_point(i60);
      const HVMesh::Node::index_type i71node = refined->add_point(i71);

      // Top Front
      nnodes[0] = ohnodes[ro[0]];
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[6] = i06node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[0]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[6] = i17node;
      nnodes[7] = i06node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[0]]);
      nnodes[1] = ohnodes[ro[1]];
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[7] = i17node;
      refined->add_elem(nnodes);

      // Top Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[0]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[5] = i06node;
      nnodes[6] = i35node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[2]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[4] = i06node;
      nnodes[5] = i17node;
      nnodes[6] = i24node;
      nnodes[7] = i35node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[3]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[1]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[4] = i17node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[7] = i24node;
      refined->add_elem(nnodes);

      // Top Back
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[0]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[2]]);
      nnodes[3] = ohnodes[ro[3]];
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      nnodes[5] = i35node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[2]]);
      nnodes[4] = i35node;
      nnodes[5] = i24node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[0]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[1]]);
      nnodes[2] = ohnodes[ro[2]];
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[3]]);
      nnodes[4] = i24node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[6]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      // Front
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[2] = i06node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[0]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[6] = i42node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[5]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[2] = i17node;
      nnodes[3] = i06node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[6] = i53node;
      nnodes[7] = i42node;
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[4]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[5]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[3] = i17node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[1]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[7] = i53node;
      refined->add_elem(nnodes);

      // Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[0]], onodes[ro[7]]);
      nnodes[1] = i06node;
      nnodes[2] = i35node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      nnodes[5] = i42node;
      nnodes[6] = i71node;
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      refined->add_elem(nnodes);

      nnodes[0] = i06node;
      nnodes[1] = i17node;
      nnodes[2] = i24node;
      nnodes[3] = i35node;
      nnodes[4] = i42node;
      nnodes[5] = i53node;
      nnodes[6] = i60node;
      nnodes[7] = i71node;
      refined->add_elem(nnodes);

      nnodes[0] = i17node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[1]], onodes[ro[6]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[3] = i24node;
      nnodes[4] = i53node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[7] = i60node;
      refined->add_elem(nnodes);

      // Back
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[4]]);
      nnodes[1] = i35node;
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[7]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      nnodes[5] = i71node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[3]]);
      refined->add_elem(nnodes);

      nnodes[0] = i35node;
      nnodes[1] = i24node;
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[3]], onodes[ro[6]]);
      nnodes[4] = i71node;
      nnodes[5] = i60node;
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      refined->add_elem(nnodes);

      nnodes[0] = i24node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[5]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[6]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[2]], onodes[ro[7]]);
      nnodes[4] = i60node;
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[2]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      refined->add_elem(nnodes);

      // Bottom Front
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[0]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[2] = i42node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      nnodes[4] = ohnodes[ro[4]];
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[6]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[1]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[2] = i53node;
      nnodes[3] = i42node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[5]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[4]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[7]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[6]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[0]]);
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[3] = i53node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[4]]);
      nnodes[5] = ohnodes[ro[5]];
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[6]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[7]]);
      refined->add_elem(nnodes);

      // Bottom Center
      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[3]]);
      nnodes[1] = i42node;
      nnodes[2] = i71node;
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[7]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[6]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[5]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[4]]);
      refined->add_elem(nnodes);

      nnodes[0] = i42node;
      nnodes[1] = i53node;
      nnodes[2] = i60node;
      nnodes[3] = i71node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[4]], onodes[ro[6]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[7]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[4]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[5]]);
      refined->add_elem(nnodes);

      nnodes[0] = i53node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[2]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[3] = i60node;
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[7]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[5]], onodes[ro[6]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[5]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[4]]);
      refined->add_elem(nnodes);

      nnodes[0] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[0]]);
      nnodes[1] = i71node;
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[5]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[6]]);
      nnodes[7] = ohnodes[ro[7]];
      refined->add_elem(nnodes);

      nnodes[0] = i71node;
      nnodes[1] = i60node;
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[2]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[5]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[4]]);
      nnodes[6] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[7]]);
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[7]], onodes[ro[6]]);
      refined->add_elem(nnodes);

      nnodes[0] = i60node;
      nnodes[1] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[1]]);
      nnodes[2] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[2]]);
      nnodes[3] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[3]]);
      nnodes[4] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[4]]);
      nnodes[5] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[5]]);
      nnodes[6] = ohnodes[ro[6]];
      nnodes[7] = lookup(mesh, refined, edgemap, onodes[ro[6]], onodes[ro[7]]);
      refined->add_elem(nnodes);
    }
    else
    {
      // non convex, emit error.
      cout << "Element not convex, cannot replace.\n";
    }
    ++bi;
  }

  GenericField<HVMesh, HexTrilinearLgn<double>, vector<double> > *ofield =
    scinew GenericField<HVMesh, HexTrilinearLgn<double>, vector<double> >(refined);
  ofield->copy_properties(fieldh.get_rep());
  return ofield;
}



class SCISHARE IRMakeLinearAlgo : public DynamicAlgoBase
{
public:

  virtual FieldHandle execute(ProgressReporter *reporter,
                              FieldHandle fieldh) = 0;

  //! support the dynamically compiled algorithm concept
  static CompileInfoHandle get_compile_info(const TypeDescription *fsr);
};


template <class IFIELD, class OFIELD>
class IRMakeLinearAlgoT : public IRMakeLinearAlgo
{
public:
  //! virtual interface. 
  virtual FieldHandle execute(ProgressReporter *reporter, FieldHandle fieldh);
};


template <class IFIELD, class OFIELD>
FieldHandle
IRMakeLinearAlgoT<IFIELD, OFIELD>::execute(ProgressReporter *reporter,
                                           FieldHandle fieldh)
{
  IFIELD *ifield = dynamic_cast<IFIELD*>(fieldh.get_rep());
  typename IFIELD::mesh_type *imesh = ifield->get_typed_mesh().get_rep();
  OFIELD *ofield = scinew OFIELD(imesh);

  typename IFIELD::mesh_type::Node::array_type nodes;
  typename IFIELD::value_type val;

  typename IFIELD::mesh_type::Elem::iterator itr, eitr;
  imesh->begin(itr);
  imesh->end(eitr);

  while (itr != eitr)
  {
    ifield->value(val, *itr);
    if (val > 0.5)
    {
      imesh->get_nodes(nodes, *itr);
      for (unsigned int i = 0; i < nodes.size(); i++)
      {
        ofield->set_value(val, nodes[i]);
      }
    }
    ++itr;
  }
  
  return ofield;
}


class SCISHARE IRMakeConvexAlgo : public DynamicAlgoBase
{
public:

  virtual void execute(ProgressReporter *reporter, FieldHandle fieldh) = 0;

  //! support the dynamically compiled algorithm concept
  static CompileInfoHandle get_compile_info(const TypeDescription *fsr);

  int pattern_table[256][2];

  inline unsigned int iedge(unsigned int a, unsigned int b)
  {
    return (1<<(7-a)) | (1<<(7-b));
  }

  inline unsigned int iface(unsigned int a, unsigned int b,
                            unsigned int c, unsigned int d)
  {
    return iedge(a, b) | iedge(c, d);
  }

  inline void set_table(int i, int pattern, int reorder)
  {
    pattern_table[i][0] = pattern;
    pattern_table[i][1] = reorder;
  }

  inline void set_table_once(int i, int pattern, int reorder)
  {
    if (pattern_table[i][0] < 0)
    {
      pattern_table[i][0] = pattern;
      pattern_table[i][1] = reorder;
    }
  }

  inline void set_iface_partials(unsigned int a, unsigned int b,
                                 unsigned int c, unsigned int d,
                                 int pattern, int reorder)
  {
    set_table_once(iedge(a, b), pattern, reorder);
    set_table_once(iedge(a, c), pattern, reorder);
    set_table_once(iedge(a, d), pattern, reorder);
    set_table_once(iedge(b, c), pattern, reorder);
    set_table_once(iedge(b, d), pattern, reorder);
    set_table_once(iedge(c, d), pattern, reorder);
    set_table(iface(b, b, c, d), pattern, reorder);
    set_table(iface(a, c, c, d), pattern, reorder);
    set_table(iface(a, b, d, d), pattern, reorder);
    set_table(iface(a, b, c, a), pattern, reorder);
  }

  void init_pattern_table()
  {
    for (int i = 0; i < 256; i++)
    {
      set_table(i, -1, 0);
    }

    set_table(0, 0, 0);

    // Add corners
    set_table(1, 1, 7);
    set_table(2, 1, 6);
    set_table(4, 1, 5);
    set_table(8, 1, 4);
    set_table(16, 1, 3);
    set_table(32, 1, 2);
    set_table(64, 1, 1);
    set_table(128, 1, 0);

    // Add edges
    set_table(iedge(0, 1), 2, 0);
    set_table(iedge(1, 2), 2, 1);
    set_table(iedge(2, 3), 2, 2);
    set_table(iedge(3, 0), 2, 3);
    set_table(iedge(4, 5), 2, 5);
    set_table(iedge(5, 6), 2, 6);
    set_table(iedge(6, 7), 2, 7);
    set_table(iedge(7, 4), 2, 4);
    set_table(iedge(0, 4), 2, 8);
    set_table(iedge(1, 5), 2, 9);
    set_table(iedge(2, 6), 2, 10);
    set_table(iedge(3, 7), 2, 11);

    set_table(iface(0, 1, 2, 3), 4, 0);
    set_table(iface(0, 1, 5, 4), 4, 12);
    set_table(iface(1, 2, 6, 5), 4, 9);
    set_table(iface(2, 3, 7, 6), 4, 13);
    set_table(iface(3, 0, 4, 7), 4, 8);
    set_table(iface(4, 5, 6, 7), 4, 7);

    set_iface_partials(0, 1, 2, 3, -4, 0);
    set_iface_partials(0, 1, 5, 4, -4, 12);
    set_iface_partials(1, 2, 6, 5, -4, 9);
    set_iface_partials(2, 3, 7, 6, -4, 13);
    set_iface_partials(3, 0, 4, 7, -4, 8);
    set_iface_partials(4, 5, 6, 7, -4, 7);

    set_table(255, 8, 0);
  }
};


template <class FIELD>
class IRMakeConvexAlgoT : public IRMakeConvexAlgo
{
public:
  //! virtual interface. 
  virtual void execute(ProgressReporter *reporter, FieldHandle fieldh);
};


template <class FIELD>
void
IRMakeConvexAlgoT<FIELD>::execute(ProgressReporter *reporter,
                                  FieldHandle fieldh)
{
  const bool lte = true;
  const double isoval = 0.5;
  double newval = 0.0;

  FIELD *field = dynamic_cast<FIELD*>(fieldh.get_rep());
  typename FIELD::mesh_type *mesh = field->get_typed_mesh().get_rep();

  init_pattern_table();
  
  typename FIELD::mesh_type::Node::array_type onodes(8);
  typename FIELD::value_type v[8];
  
  bool changed;
  do {
    newval -= 1.0;
    changed = false;
    typename FIELD::mesh_type::Elem::iterator bi, ei;
    mesh->begin(bi); mesh->end(ei);
    while (bi != ei)
    {
      mesh->get_nodes(onodes, *bi);
    
      // Get the values and compute an inside/outside mask.
      unsigned int inside = 0;
      unsigned int inside_count = 0;
      for (unsigned int i = 0; i < onodes.size(); i++)
      {
        field->value(v[i], onodes[i]);
        inside = inside << 1;
        if (v[i] > isoval)
        {
          inside |= 1;
          inside_count++;
        }
      }

      // Invert the mask if we are doing less than.
      if (lte) { inside = ~inside & 0xff; inside_count = 8 - inside_count; }

      const int pattern = pattern_table[inside][0];
      const int which = pattern_table[inside][1];

      if (pattern == -1)
      {
        changed = true;
        for (unsigned int i = 0; i < onodes.size(); i++)
        {
          field->set_value(newval, onodes[i]);
        }
      }
      else if (pattern == -4)
      {
        changed = true;
        const int *ro = IsoRefineAlgo::hex_reorder_table[which];

        for (unsigned int i = 0; i < 4; i++)
        {
          field->set_value(newval, onodes[ro[i]]);
        }
      }

      ++bi;
    }
  } while (changed);
}


} // namespace SCIRun

#endif // ClipField_h
