
/*
 *  LaceContours.cc:  Lace a ContourSet into a Surface
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <LaceContours/LaceContours.h>
#include <ContourSet.h>
#include <Surface.h>
#include <SurfacePort.h>
#include <ContourSetPort.h>
#include <ModuleList.h>
#include <NotFinished.h>
#include <iostream.h>
#include <fstream.h>
#include <Math/MiscMath.h>

#define Sqr(x) ((x)*(x))
static Module* make_LaceContours(const clString& id)
{
    return new LaceContours(id);
}

static RegisterModule db1("Contours", "Lace Contours", make_LaceContours);
static RegisterModule db2("Visualization", "Lace Contours", make_LaceContours);

LaceContours::LaceContours(const clString& id)
: Module("LaceContours", id, Filter)
{
    // Create the input port
    incontour=new ContourSetIPort(this, "ContourSet", ContourSetIPort::Atomic);
    add_iport(incontour);
    osurface=new SurfaceOPort(this, "Surface", SurfaceIPort::Atomic);
    add_oport(osurface);
}

LaceContours::LaceContours(const LaceContours&copy, int deep)
: Module(copy, deep)
{
    NOT_FINISHED("LaceContours::LaceContours");
}

LaceContours::~LaceContours()
{
}

Module* LaceContours::clone(int deep)
{
    return new LaceContours(*this, deep);
}

void LaceContours::execute()
{
    ContourSetHandle contours;
    if (!incontour->get(contours))
	return;
    TriSurface* surf=new TriSurface;
    surf->name=contours->name;
    lace_contours(contours, surf);
    osurface->send(surf);
}

void LaceContours::lace_contours(const ContourSetHandle& contour, 
				 TriSurface* surf) {
    Array1<int> row;
    for (int i=0, curr=0; i<contour->contours.size(); i++) {
	row.add(curr);
	curr+=contour->contours[i].size();
	for (int j=0; j<contour->contours[i].size(); j++) {
	    Point p(contour->contours[i][j]-contour->origin);
	    p=Point(0,0,0)+contour->basis[0]*p.x()+contour->basis[1]*p.y()+
		contour->basis[2]*(p.z()*contour->space);
	    surf->add_point(p);
	}
    }
   // i will be the index of the top contour being laced, i-1 being the other
   for (i=1; i<contour->contours.size(); i++) {
       int top=0;
       double dtemp;
       int sz_top=contour->contours[i].size();
       int sz_bot=contour->contours[i-1].size();
       if ((sz_top < 2) && (sz_bot < 2)) {
	   cerr << "Not enough points to lace!\n";
	   return;
       }
       // 0 will be the index of our first bottom point, set top to be the 
       // index of the closest top point to it
       double dist=Sqr(contour->contours[i][0].x()-
		       contour->contours[i-1][0].x())+
		   Sqr(contour->contours[i][0].y()-
		       contour->contours[i-1][0].y());
       for (int start=1; start<sz_top; start++) {
	   if ((dtemp=(Sqr(contour->contours[i][start].x()-
			   contour->contours[i-1][0].x())+
		       Sqr(contour->contours[i][start].y()-
			   contour->contours[i-1][0].y())))<dist) {
	       top=start;
	       dist=dtemp;
	   }
       }
       int bot=0;
       // lets start lacing...  top and bottom will always store the indices
       // of the first matched points so we know when to stop
       int jdone=(sz_top==1); // does this val have to change for us to
       int kdone=(sz_bot==1); // be done lacing
       for (int j=top,k=bot; !jdone || !kdone;) {
	   double d1=Sqr(contour->contours[i][j].x()-
			 contour->contours[i-1][(k+1)%sz_bot].x())+
	       Sqr(contour->contours[i][j].y()-
		   contour->contours[i-1][(k+1)%sz_bot].y());
	   double d2=Sqr(contour->contours[i][(j+1)%sz_top].x()-
			 contour->contours[i-1][k].x())+
	             Sqr(contour->contours[i][(j+1)%sz_top].y()-
			 contour->contours[i-1][k].y());
	   if ((d1<d2 || jdone) && !kdone) { // bottom moves
	       surf->add_triangle(row[i]+j,row[i-1]+k,row[i-1]+((k+1)%sz_bot));
	       k=(k+1)%sz_bot;
	       if (k==bot) kdone=1;
	   } else {     // top moves
	       surf->add_triangle(row[i]+j,row[i-1]+k,row[i]+((j+1)%sz_top));
	       j=(j+1)%sz_top;
	       if (j==top) jdone=1;
	   }
       }
   }
}
