
/*
 * Manual template instantiations for g++
 */

#include <SCICore/Containers/Array1.h>
#include <SCICore/Containers/Array2.h>
#include <SCICore/Containers/Array3.h>
#include <SCICore/Containers/HashTable.h>
#include <SCICore/Containers/LockingHandle.h>
#include <SCICore/Containers/Queue.h>
#include <SCICore/Multitask/AsyncReply.h>
#include <SCICore/Multitask/Mailbox.h>
#include <SCICore/CoreDatatypes/cMatrix.h>
#include <SCICore/CoreDatatypes/cVector.h>
#include <SCICore/CoreDatatypes/Boolean.h>
#include <SCICore/CoreDatatypes/ColorMap.h>
#include <SCICore/CoreDatatypes/ColumnMatrix.h>
#include <SCICore/CoreDatatypes/ContourSet.h>
#include <SCICore/CoreDatatypes/DenseMatrix.h>
#include <SCICore/CoreDatatypes/HexMesh.h>
#include <SCICore/CoreDatatypes/Image.h>
#include <SCICore/CoreDatatypes/Interval.h>
#include <SCICore/CoreDatatypes/LockArray3.h>
#include <SCICore/CoreDatatypes/ScalarField.h>
#include <SCICore/CoreDatatypes/Surface.h>
#include <SCICore/CoreDatatypes/SurfTree.h>
#include <SCICore/CoreDatatypes/VectorField.h>
#include <SCICore/CoreDatatypes/VoidStar.h>

using namespace SCICore::PersistentSpace;
using namespace SCICore::Containers;
using namespace SCICore::CoreDatatypes;
using namespace SCICore::GeomSpace;

template class Array1<Array1<double> >;
template class Array1<ScalarFieldHandle>;
template class Array1<TSElement*>;
template class Array1<TSEdge*>;
template class Array1<SurfInfo>;
template class Array1<FaceInfo>;
template class Array1<EdgeInfo>;
template class Array1<NodeInfo>;
template class Array1<Array1<int> >;
template class Array1<Array1<Array1<int> > >;
template class Array1<VectorFieldHandle>;
template class Array1<Hexahedron*>;
template class Array1<Array1<Vector> >;
template class Array1<Array3<short> >;
template class Array1<VectorFieldRG*>;

template class Array2<Color>;

template class Array3<double>;
template class Array3<short>;
template class Array3<float>;

template void Pio<>(Piostream&, Array1<Color>&);
template void Pio<>(Piostream&, Array1<Array1<Point> >&);
template void Pio<>(Piostream&, Array3<Array1<int> >&);
template void Pio<>(Piostream&, Array1<NodeVersion1>&);
template void Pio<>(Piostream&, Array1<NodeHandle>&);
template void Pio<>(Piostream&, Array1<ElementVersion1>&);
template void Pio<>(Piostream&, Array1<Element*>&);
template void Pio<>(Piostream&, Array1<Array1<double> >&);
template void Pio<>(Piostream&, SurfaceHandle&);
template void Pio<>(Piostream&, Array1<MeshHandle>&);
template void Pio<>(Piostream&, Array1<DenseMatrix>&);
template void Pio<>(Piostream&, Array3<double>&);
template void Pio<>(Piostream&, Array3<char>&);
template void Pio<>(Piostream&, Array3<short>&);
template void Pio<>(Piostream&, Array3<int>&);
template void Pio<>(Piostream&, Array3<float>&);
template void Pio<>(Piostream&, Array3<unsigned char>&);
template void Pio<>(Piostream&, Array1<ScalarFieldHandle>&);
template void Pio<>(Piostream&, Array1<clString>&);
template void Pio<>(Piostream&, Array1<TSElement*>&);
template void Pio<>(Piostream&, Array1<TSEdge*>&);
template void Pio<>(Piostream&, Array1<SurfInfo>&);
template void Pio<>(Piostream&, Array1<FaceInfo>&);
template void Pio<>(Piostream&, Array1<EdgeInfo>&);
template void Pio<>(Piostream&, Array1<NodeInfo>&);
template void Pio<>(Piostream&, Array1<Array1<int> >&);
template void Pio<>(Piostream&, Array1<Array1<Array1<int> > >&);
template void Pio<>(Piostream&, Array1<VectorFieldHandle>&);
template void Pio<>(Piostream&, Array3<Vector>&);
template void Pio<>(Piostream&, HashTable<int, HexNode*>&);
template void Pio<>(Piostream&, HashTable<int, HexFace*>&);
template void Pio<>(Piostream&, HashTable<int, Hexahedron*>&);
template void Pio<>(Piostream&, Array1<Array1<Vector> >&);

template class HashTable<int, int>;
template class HashTable<int, HexNode*>;
template class HashTableIter<int, HexNode*>;
template class HashTable<int, Hexahedron*>;
template class HashTableIter<int, Hexahedron*>;
template class HashTable<int, HexFace*>;
template class HashTableIter<int, HexFace*>;
template class HashTable<FourHexNodes, HexFace*>;


