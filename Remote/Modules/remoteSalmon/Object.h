//////////////////////////////////////////////////////////////////////
// Object.h - Represent a single begin/end pair.
//
// Copyright David K. McAllister July 1999.
//
//////////////////////////////////////////////////////////////////////

#ifndef _object_h
#define _object_h

#include <Remote/Modules/remoteSalmon/Image.h>
#include <Remote/Modules/remoteSalmon/BBox.h>
#include <Remote/Modules/remoteSalmon/Matrix44.h>
#include <SCICore/Geometry/Vector.h>

#include <vector>
using namespace std;

// Ripped directly from <GL/gl.h>
#define L_POINTS                           0x0000
#define L_LINES                            0x0001
#define L_LINE_LOOP                        0x0002
#define L_LINE_STRIP                       0x0003
#define L_TRIANGLES                        0x0004
#define L_TRIANGLE_STRIP                   0x0005
#define L_TRIANGLE_FAN                     0x0006
#define L_QUADS                            0x0007
#define L_QUAD_STRIP                       0x0008
#define L_POLYGON                          0x0009

struct Object
{
  char Name[64];
  dVector scolor, ecolor, acolor;
  double shininess, creaseAngle; // Used for later generation of normals.
  int PrimType; // GL_TRIANGLE_STRIP, etc.
  int ObjID; // OpenGL display list ID.
  int TexInd; // Index into the texture database.
  bool SColorValid, EColorValid, AColorValid, ShininessValid;
  BBox Box;

  vector<dVector> verts;
  vector<dVector> normals;   // Must have a length of 0, 1, or verts.size().
  vector<dVector> texcoords; // Must have a length of 0, 1, or verts.size().
  vector<dVector> dcolors;   // Must have a length of 0, 1, or verts.size().
  vector<double> alpha;     // Must have a length of 0 or dcolors.size().

  inline Object()
  {
    //cerr << "Making Object.\n";
    Name[0] = '\0';
    TexInd = ObjID = -1;
    SColorValid = EColorValid = AColorValid = ShininessValid = false;
    PrimType = 4;
    creaseAngle = M_PI * 0.5;
    shininess = 32.0;
    scolor = dVector(0,0,0);
    ecolor = dVector(0,0,0);
    acolor = dVector(0.2,0.2,0.2);
  }
    
  void Dump() const;
  
  void GenNormals(double CreaseAngle);

  void MakeTriangles(bool KeepBad = true);

  void RebuildBBox();

  void RemoveTriangles(dVector& pos, double thresh=0.05);
  
};

#endif
