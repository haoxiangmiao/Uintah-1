// This is an autamatically generated file, do not edit!
#include "/usr/local/SCIRun/src/Core/Datatypes/HexVolMesh.h"
#include "/usr/local/SCIRun/src/Dataflow/Modules/Fields/FieldBoundary.h"
using namespace SCIRun;

extern "C" {
FieldBoundaryAlgoAux* maker() {
  return scinew FieldBoundaryAlgoQuadT<HexVolMesh>;
}
}
