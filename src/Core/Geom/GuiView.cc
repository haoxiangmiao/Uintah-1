/*
 *  GuiView.cc  Structure that provides for easy access of view information.
 *              The view information is interactively provided by the user.
 *
 *  Written by:
 *   Steven Parker
 *   Department of Computer Science
 *   University of Utah
 *
 *   separated from the Viewer code by me (Aleksandra)
 *   in May 1996
 *
 *  Copyright (C) 1996 SCI Group
 */

#include <tcl.h>

#include <Core/GuiInterface/TCL.h>
#include <Core/GuiInterface/TCLTask.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geom/GuiView.h>
#include <iostream>
using std::ostream;

namespace SCIRun {

GuiView::GuiView(const clString& name, const clString& id, TCL* tcl)
: GuiVar(name, id, tcl), eyep("eyep", str(), tcl),
  lookat("lookat", str(), tcl), up("up", str(), tcl),
  fov("fov", str(), tcl), eyep_offset("eyep_offset", str(), tcl)
{
}

GuiView::~GuiView()
{
}

void GuiView::reset() {
  eyep.reset();
  lookat.reset();
  up.reset();
  fov.reset();
  eyep_offset.reset();
}

View
GuiView::get()
{
    TCLTask::lock();
    View v(eyep.get(), lookat.get(), up.get(), fov.get());
    TCLTask::unlock();
    return v;
}

void
GuiView::set(const View& view)
{
    TCLTask::lock();
    eyep.set(view.eyep());
    lookat.set(view.lookat());
    up.set(view.up());
    fov.set(view.fov());
    TCLTask::unlock();
}


void
GuiView::emit(ostream& out, clString& midx)
{
    eyep.emit(out, midx);
    lookat.emit(out, midx);
    up.emit(out, midx);
    fov.emit(out, midx);
}



GuiExtendedView::GuiExtendedView( const clString& name, const clString& id,
				 TCL* tcl )
: GuiVar(name, id, tcl), eyep("eyep", str(), tcl),
  lookat("lookat", str(), tcl), up("up", str(), tcl),
  fov("fov", str(), tcl), eyep_offset("eyep_offset", str(), tcl),
  xres("xres", str(), tcl), yres("yres", str(), tcl), bg("bg", str(), tcl)
{
}

GuiExtendedView::~GuiExtendedView()
{
}


void GuiExtendedView::reset() {
  eyep.reset();
  lookat.reset();
  up.reset();
  fov.reset();
  eyep_offset.reset();
  xres.reset();
  yres.reset();
  bg.reset();
}

ExtendedView
GuiExtendedView::get()
{
    TCLTask::lock();
    ExtendedView v(eyep.get(), lookat.get(), up.get(), fov.get(), xres.get(),
		   yres.get(), bg.get()*( 1. / 255 ) );
    TCLTask::unlock();
    return v;
}

void
GuiExtendedView::set(const ExtendedView& view)
{
    TCLTask::lock();
    eyep.set(view.eyep());
    lookat.set(view.lookat());
    up.set(view.up());
    fov.set(view.fov());
    xres.set(view.xres());
    yres.set(view.yres());
    bg.set( view.bg()*255 );
    TCLTask::unlock();
}


void
GuiExtendedView::emit(ostream& out, clString& midx)
{
    eyep.emit(out, midx);
    lookat.emit(out, midx);
    up.emit(out, midx);
    fov.emit(out, midx);
    xres.emit(out, midx);
    yres.emit(out, midx);
    bg.emit(out, midx);
}

} // End namespace SCIRun

