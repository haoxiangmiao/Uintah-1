
/*
 *  TransformField.cc:  Rotate and flip field to get it into "standard" view
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   December 1995
 *
 *  Copyright (C) 1995 SCI Group
 */

#include <Core/Containers/String.h>
#include <Dataflow/Network/Module.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <Dataflow/Ports/MatrixPort.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Geometry/Transform.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Math/MiscMath.h>
#include <iostream>
using std::cerr;
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace SCIRun {


class TransformField : public Module {
    ScalarFieldIPort *iport;
    MatrixIPort *imat;
    ScalarFieldOPort *oport;
    void MatToTransform(MatrixHandle mH, Transform& t);
public:
    TransformField(const clString& id);
    virtual ~TransformField();
    virtual void execute();
};

extern "C" Module* make_TransformField(const clString& id) {
  return new TransformField(id);
}

TransformField::TransformField(const clString& id)
: Module("TransformField", id, Source)
{
    // Create the input port
    iport = scinew ScalarFieldIPort(this, "SFRG", ScalarFieldIPort::Atomic);
    add_iport(iport);
    imat = scinew MatrixIPort(this, "Matrix", MatrixIPort::Atomic);
    add_iport(imat);
    oport = scinew ScalarFieldOPort(this, "SFRG",ScalarFieldIPort::Atomic);
    add_oport(oport);
}

TransformField::~TransformField()
{
}

void TransformField::MatToTransform(MatrixHandle mH, Transform& t) {
    double a[16];
    double *p=&(a[0]);
    for (int i=0; i<4; i++)
        for (int j=0; j<4; j++)
            *p++=(*mH.get_rep())[i][j];
    t.set(a);
}

void TransformField::execute()
{

    ScalarFieldHandle sfIH;
    iport->get(sfIH);
    if (!sfIH.get_rep()) return;
    ScalarFieldRGBase *sfrgb;
    if ((sfrgb=sfIH->getRGBase()) == 0) return;

    MatrixHandle mIH;
    imat->get(mIH);
    if (!mIH.get_rep()) return;
    if ((mIH->nrows() != 4) || (mIH->ncols() != 4)) return;
    Transform t;
    MatToTransform(mIH, t);

    ScalarFieldRGdouble *ifd, *ofd;
    ScalarFieldRGfloat *iff, *off;
    ScalarFieldRGint *ifi, *ofi;
    ScalarFieldRGshort *ifs, *ofs;
    ScalarFieldRGuchar *ifu, *ofu;
    ScalarFieldRGchar *ifc, *ofc;
    
    ScalarFieldRGBase *ofb;

    ifd=sfrgb->getRGDouble();
    iff=sfrgb->getRGFloat();
    ifi=sfrgb->getRGInt();
    ifs=sfrgb->getRGShort();
    ifu=sfrgb->getRGUchar();
    ifc=sfrgb->getRGChar();
    
    ofd=0;
    off=0;
    ofs=0;
    ofi=0;
    ofc=0;

    int nx=sfrgb->nx;
    int ny=sfrgb->ny;
    int nz=sfrgb->nz;
    Point min;
    Point max;
    sfrgb->get_bounds(min, max);
    if (ifd) {
	ofd=scinew ScalarFieldRGdouble(); 
	ofd->resize(nx,ny,nz);
	ofb=ofd;
    } else if (iff) {
	off=scinew ScalarFieldRGfloat(); 
	off->resize(nx,ny,nz);
	ofb=off;
    } else if (ifi) {
	ofi=scinew ScalarFieldRGint(); 
	ofi->resize(nx,ny,nz);
	ofb=ofi;
    } else if (ifs) {
	ofs=scinew ScalarFieldRGshort(); 
	ofs->resize(nx,ny,nz);
	ofb=ofs;
    } else if (ifu) {
	ofu=scinew ScalarFieldRGuchar(); 
	ofu->resize(nx,ny,nz);
	ofb=ofu;
    } else if (ifc) {
	ofc=scinew ScalarFieldRGchar(); 
	ofc->resize(nx,ny,nz);
	ofb=ofc;
    }
    ofb->set_bounds(Point(min.x(), min.y(), min.z()), 
		    Point(max.x(), max.y(), max.z()));
    for (int i=0; i<nx; i++)
	for (int j=0; j<ny; j++)
	    for (int k=0; k<nz; k++) {
                Point oldp(sfrgb->get_point(i,j,k));
                Point newp(t.unproject(oldp));
                double val=0;
		if (ifd) { 
		    ifd->interpolate(newp, val); 
		    ofd->grid(i,j,k)=val;
		} else if (iff) {
		    iff->interpolate(newp, val);
		    off->grid(i,j,k)=(float)val;
		} else if (ifi) {
		    ifi->interpolate(newp, val);
		    ofi->grid(i,j,k)=(int)val;
		} else if (ifs) {
		    ifs->interpolate(newp, val);
		    ofs->grid(i,j,k)=(short)val;
		} else if (ifu) {
		    ifu->interpolate(newp, val);
		    ofu->grid(i,j,k)=(unsigned char)val;
		} else if (ifi) {
		    ifc->interpolate(newp, val);
		    ofc->grid(i,j,k)=(char)val;
		}
	    }
    ScalarFieldHandle sfOH(ofb);
    oport->send(sfOH);
}

} // End namespace SCIRun

