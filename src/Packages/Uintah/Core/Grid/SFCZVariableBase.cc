#include <Packages/Uintah/Core/Grid/SFCZVariableBase.h>
#include <Packages/Uintah/Core/Grid/TypeDescription.h>
#include <Packages/Uintah/Core/Grid/BufferInfo.h>
#include <Core/Geometry/IntVector.h>
#include <Core/Exceptions/InternalError.h>

using namespace Uintah;
using namespace SCIRun;

SFCZVariableBase::~SFCZVariableBase()
{
}

SFCZVariableBase::SFCZVariableBase()
{
}

void SFCZVariableBase::getMPIBuffer(BufferInfo& buffer,
				    const IntVector& low, const IntVector& high)
{
  const TypeDescription* td = virtualGetTypeDescription()->getSubType();
  MPI_Datatype basetype=td->getMPIType();
  IntVector l, h, s, strides, dataLow;
  getSizes(l, h, dataLow, s, strides);
  IntVector off = low - dataLow;
  
  char* startbuf = (char*)getBasePointer();
  startbuf += strides.x()*off.x()+strides.y()*off.y()+strides.z()*off.z();
  IntVector d = high-low;
  MPI_Datatype type1d;
  MPI_Type_hvector(d.x(), 1, strides.x(), basetype, &type1d);
  MPI_Datatype type2d;
  MPI_Type_hvector(d.y(), 1, strides.y(), type1d, &type2d);
  MPI_Type_free(&type1d);
  MPI_Datatype type3d;
  MPI_Type_hvector(d.z(), 1, strides.z(), type2d, &type3d);
  MPI_Type_free(&type2d);
  MPI_Type_commit(&type3d);
  buffer.add(startbuf, 1, type3d, true);
}
