/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/
#include <sci_defs.h>
#include <Core/GLVolumeRenderer/GLOverOp.h>
#if defined( HAVE_GLEW )
#include <GL/glew.h>
#else
#include <GL/gl.h>
#endif


namespace SCIRun {


GLOverOp::GLOverOp(const GLVolumeRenderer* glvr) :
  GLTexRenState( glvr )
{
}

void GLOverOp::preDraw()
{
  // comment out blending, done in render algorithm
//   glEnable(GL_BLEND);
  // This is the default, but just make sure
  glBlendEquation(GL_FUNC_ADD_EXT);

  // Use the following if not premultiplying alpha
//    glBlendFunc( GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  // since we premultiply alpha we use the following
  glBlendFunc( GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
}

void GLOverOp::postDraw()
{
  // comment out blending, done in render algorithm
//   glDisable(GL_BLEND);
}


} // End namespace SCIRun
