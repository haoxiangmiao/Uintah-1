
/*
 *  SurfToGeom.cc:  Convert a surface into geoemtry
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Classlib/NotFinished.h>
#include <Dataflow/Module.h>
#include <Dataflow/ModuleList.h>
#include <Datatypes/GeometryPort.h>
#include <Datatypes/ScalarFieldPort.h>
#include <Datatypes/Surface.h>
#include <Datatypes/SurfacePort.h>
#include <Datatypes/TriSurface.h>
#include <Geom/Color.h>
#include <Geom/Geom.h>
#include <Geom/Group.h>
#include <Geom/Tri.h>

class SurfToGeom : public Module {
    SurfaceIPort* isurface;
    GeometryOPort* ogeom;

    void surf_to_geom(const SurfaceHandle&, GeomGroup*);
public:
    SurfToGeom(const clString& id);
    SurfToGeom(const SurfToGeom&, int deep);
    virtual ~SurfToGeom();
    virtual Module* clone(int deep);
    virtual void execute();
};

static Module* make_SurfToGeom(const clString& id)
{
    return new SurfToGeom(id);
}

static RegisterModule db1("Surfaces", "Surface to Geometry", make_SurfToGeom);
static RegisterModule db2("Visualization", "Surface to Geometry", 
			  make_SurfToGeom);
static RegisterModule db3("Dave", "Surface to Geometry", make_SurfToGeom);

SurfToGeom::SurfToGeom(const clString& id)
: Module("SurfToGeom", id, Filter)
{
    // Create the input port
    isurface=new SurfaceIPort(this, "Surface", SurfaceIPort::Atomic);
    add_iport(isurface);
    add_iport(new ScalarFieldIPort(this, "ScalarField", ScalarFieldIPort::Atomic));
    ogeom=new GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
    add_oport(ogeom);
}

SurfToGeom::SurfToGeom(const SurfToGeom&copy, int deep)
: Module(copy, deep)
{
    NOT_FINISHED("SurfToGeom::SurfToGeom");
}

SurfToGeom::~SurfToGeom()
{
}

Module* SurfToGeom::clone(int deep)
{
    return new SurfToGeom(*this, deep);
}

void SurfToGeom::execute()
{
    SurfaceHandle surf;
    if (!isurface->get(surf))
	return;

    GeomGroup* group = new GeomGroup;
    TriSurface* ts=surf->getTriSurface();
    if(ts){
	for (int i=0; i<ts->elements.size(); i++) {
	    group->add(new GeomTri(ts->points[ts->elements[i]->i1], 
				   ts->points[ts->elements[i]->i2],
				   ts->points[ts->elements[i]->i3]));
	}
    } else {
	error("Unknown representation for Surface in SurfToGeom");
    }
    if (surf->name == "sagital.scalp") {
	group->set_matl(new Material(Color(0,0,0), Color(0,.6,0), 
				     Color(.5,.5,.5), 20));
    }
    ogeom->delAll();
    ogeom->addObj(group, surf->name);
}
