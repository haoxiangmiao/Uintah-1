//static char *id="@(#) $Id$";

/*
 *  Transform.cc: ?
 *
 *  Written by:
 *   Author ?
 *   Department of Computer Science
 *   University of Utah
 *   Date ?
 *
 *  Copyright (C) 199? SCI Group
 */

#include <SCICore/Geometry/Transform.h>
#include <SCICore/Geometry/Point.h>
#include <SCICore/Geometry/Vector.h>
#include <SCICore/Math/MiscMath.h>
#include <SCICore/Math/Trig.h>
#include <iostream>
using std::cerr;
#include <stdio.h>

namespace SCICore {
namespace Geometry {

Transform::Transform()
{
    load_identity();
    inverse_valid=0;
}

Transform::Transform(const Transform& copy)
{
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    mat[i][j]=copy.mat[i][j];
	    imat[i][j]=copy.imat[i][j];
	}
    }
    inverse_valid=copy.inverse_valid;
}

Transform::Transform(const Point& p, const Vector& i, const Vector& j, const Vector& k){
  load_frame(p, i, j, k);
}

Transform::~Transform()
{
}

void Transform::load_frame(const Point&,
			   const Vector& x, 
			   const Vector& y, 
			   const Vector& z)
{
    mat[3][3] = imat[3][3] = 1.0;
    mat[0][3] = mat[1][3] = mat[2][3] = 0.0; // no perspective
    imat[0][3] = imat[1][3] = imat[2][3] = 0.0; // no perspective

    mat[3][0] = mat[3][1] = mat[3][2] = 0.0;
    imat[3][0] = imat[3][1] = imat[3][2] = 0.0;

    mat[0][0] = x.x();
    mat[1][0] = x.y();
    mat[2][0] = x.z();

    mat[0][1] = y.x();
    mat[1][1] = y.y();
    mat[2][1] = y.z();

    mat[0][2] = z.x();
    mat[1][2] = z.y();
    mat[2][2] = z.z();

    imat[0][0] = x.x();
    imat[0][1] = x.y();
    imat[0][2] = x.z();

    imat[1][0] = y.x();
    imat[1][1] = y.y();
    imat[1][2] = y.z();

    imat[2][0] = z.x();
    imat[2][1] = z.y();
    imat[2][2] = z.z();

    inverse_valid = 1;
}

void Transform::change_basis(Transform& T)
{
    T.compute_imat();
    pre_mulmat(T.imat);
    post_mulmat(T.mat);
}

void Transform::post_trans(Transform& T)
{
    post_mulmat(T.mat);
}

void Transform::pre_trans(Transform& T)
{
    pre_mulmat(T.mat);
}

void Transform::print(void)
{
    for(int i=0;i<4;i++) {
	for(int j=0;j<4;j++)
	    printf("%f ",mat[i][j]); 
	printf("\n");
    }
    printf("\n");
	
}

void Transform::printi(void)
{
    for(int i=0;i<4;i++) {
	for(int j=0;j<4;j++)
	    printf("%f ",imat[i][j]); 
	printf("\n");
    }
    printf("\n");
	
}

void Transform::build_scale(double m[4][4], const Vector& v)
{
    load_identity(m);
    m[0][0]=v.x();
    m[1][1]=v.y();
    m[2][2]=v.z();
}
    
void Transform::pre_scale(const Vector& v)
{
    double m[4][4];
    build_scale(m,v);
    pre_mulmat(m);
    inverse_valid=0;
}

void Transform::post_scale(const Vector& v)
{
    double m[4][4];
    build_scale(m,v);
    post_mulmat(m);
    inverse_valid=0;
}

void Transform::build_shear(double mat[4][4], Vector n, 
			    const Vector& s, double d) {
    load_identity(mat);
    Vector u, v;
    if (n.length() == 0) n.x(1.);
    n.normalize();
    n.find_orthogonal(u, v);
    v=-v; u=-u;
    Point p;
    Transform b;
    b.load_frame(p, n, u, v);
    double projN=Dot(s,n);
    if (projN==0) return;
    double projU=Dot(s,u);
    double projV=Dot(s,v);
    Transform sh;
    double a[16];
    sh.get(a);
    a[4]=projU/projN;
    a[8]=projV/projN;
    sh.set(a);
    sh.change_basis(b);
    Vector t(u*(Dot(s,u)*-d)+v*(Dot(s,v)*-d));
    sh.post_translate(t);
    sh.get(a);
    double *ptr=&(a[0]);
    for (int i=0; i<4; i++)
	for (int j=0; j<4; j++)
	    mat[i][j]=*ptr++;
}

void Transform::pre_shear(const Vector& n, const Vector& s, double d)
{
    double m[4][4];
    build_shear(m,n,s,d);
    pre_mulmat(m);
    inverse_valid=0;
}

void Transform::post_shear(const Vector& n, const Vector& s, double d)
{
    double m[4][4];
    build_shear(m,n,s,d);
    post_mulmat(m);
    inverse_valid=0;
}

void Transform::build_translate(double m[4][4], const Vector& v)
{
    load_identity(m);
    m[0][3]=v.x();
    m[1][3]=v.y();
    m[2][3]=v.z();
}

void Transform::pre_translate(const Vector& v)
{
    double m[4][4];
    build_translate(m,v);
    pre_mulmat(m);
    inverse_valid=0;
}

void Transform::post_translate(const Vector& v)
{
    double m[4][4];
    build_translate(m,v);    
    post_mulmat(m);
    inverse_valid=0;
}

void Transform::build_rotate(double m[4][4], double angle, const Vector& axis)
{
    // From Foley and Van Dam, Pg 227
    // NOTE: Element 0,1 is wrong in the text!
    double sintheta=Sin(angle);
    double costheta=Cos(angle);
    double ux=axis.x();
    double uy=axis.y();
    double uz=axis.z();
    m[0][0]=ux*ux+costheta*(1-ux*ux);
    m[0][1]=ux*uy*(1-costheta)-uz*sintheta;
    m[0][2]=uz*ux*(1-costheta)+uy*sintheta;
    m[0][3]=0;

    m[1][0]=ux*uy*(1-costheta)+uz*sintheta;
    m[1][1]=uy*uy+costheta*(1-uy*uy);
    m[1][2]=uy*uz*(1-costheta)-ux*sintheta;
    m[1][3]=0;

    m[2][0]=uz*ux*(1-costheta)-uy*sintheta;
    m[2][1]=uy*uz*(1-costheta)+ux*sintheta;
    m[2][2]=uz*uz+costheta*(1-uz*uz);
    m[2][3]=0;

    m[3][0]=0;
    m[3][1]=0;
    m[3][2]=0;
    m[3][3]=1;
}

void Transform::pre_rotate(double angle, const Vector& axis)
{
    double m[4][4];
    build_rotate(m, angle, axis);
    pre_mulmat(m);
    inverse_valid=0;
}	

void Transform::post_rotate(double angle, const Vector& axis)
{
    double m[4][4];
    build_rotate(m, angle, axis);
    post_mulmat(m);
    inverse_valid=0;
}	

void Transform::build_permute(double m[4][4],int xmap, int ymap, int zmap, 
			      int pre){
    load_zero(m);
    m[3][3]=1;
    if (pre) {	// for each row, set the mapped column
	if (xmap<0) m[0][-1-xmap]=-1; else m[0][xmap-1]=1;
	if (ymap<0) m[1][-1-ymap]=-1; else m[1][ymap-1]=1;
	if (zmap<0) m[2][-1-zmap]=-1; else m[2][zmap-1]=1;
    } else {	// for each column, set the mapped row
	if (xmap<0) m[-1-xmap][0]=-1; else m[xmap-1][0]=1;
	if (ymap<0) m[-1-ymap][1]=-1; else m[ymap-1][1]=1;
	if (zmap<0) m[-1-zmap][2]=-1; else m[zmap-1][2]=1;
    }
}

void Transform::pre_permute(int xmap, int ymap, int zmap) {
    double m[4][4];
    build_permute(m, xmap, ymap, zmap, 1);
    pre_mulmat(m);
    inverse_valid=0;
}

void Transform::post_permute(int xmap, int ymap, int zmap) {
    double m[4][4];
    build_permute(m, xmap, ymap, zmap, 0);
    post_mulmat(m);
    inverse_valid=0;
}

Point Transform::project(const Point& p)
{
    return Point(mat[0][0]*p.x()+mat[0][1]*p.y()+mat[0][2]*p.z()+mat[0][3],
		 mat[1][0]*p.x()+mat[1][1]*p.y()+mat[1][2]*p.z()+mat[1][3],
		 mat[2][0]*p.x()+mat[2][1]*p.y()+mat[2][2]*p.z()+mat[2][3],
		 mat[3][0]*p.x()+mat[3][1]*p.y()+mat[3][2]*p.z()+mat[3][3]);
}

Vector Transform::project(const Vector& p)
{
    return Vector(mat[0][0]*p.x()+mat[0][1]*p.y()+mat[0][2]*p.z(),
		 mat[1][0]*p.x()+mat[1][1]*p.y()+mat[1][2]*p.z(),
		 mat[2][0]*p.x()+mat[2][1]*p.y()+mat[2][2]*p.z());
}

Point Transform::unproject(const Point& p)
{
    if(!inverse_valid)compute_imat();
    return Point(imat[0][0]*p.x()+imat[0][1]*p.y()+imat[0][2]*p.z()+imat[0][3],
		 imat[1][0]*p.x()+imat[1][1]*p.y()+imat[1][2]*p.z()+imat[1][3],
		 imat[2][0]*p.x()+imat[2][1]*p.y()+imat[2][2]*p.z()+imat[2][3],
		 imat[3][0]*p.x()+imat[3][1]*p.y()+imat[3][2]*p.z()+imat[3][3]);
}

void Transform::get(double* gmat)
{
    double* p=gmat;
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    *p++=mat[i][j];
	}
    }
}

// GL stores its matrices column-major.  Need to take the transpose...
void Transform::get_trans(double* gmat)
{
    double* p=gmat;
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    *p++=mat[j][i];
	}
    }
}

void Transform::set(double* pmat)
{
    double* p=pmat;
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    mat[i][j]= *p++;
	}
    }
    inverse_valid=0;
}

void Transform::load_zero(double m[4][4])
{
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    m[i][j]=0;
	}
    }
}

void Transform::load_identity()
{
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    mat[i][j]=0;
	}
	mat[i][i]=1.0;
    }
    inverse_valid=0;
}

void Transform::install_mat(double m[4][4])
{
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    mat[i][j]=m[i][j];
	}
    }
}

void Transform::load_identity(double m[4][4]) 
{
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    m[i][j]=0;
	}
	m[i][i]=1.0;
    }
}

void Transform::invert() {
    double tmp;
    compute_imat();
    for (int i=0; i<4; i++)
	for (int j=0; j<4; j++) {
	    tmp=mat[i][j];
	    mat[i][j]=imat[j][i];
	    imat[j][i]=tmp;
	}
}

void Transform::compute_imat()
{
    double a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p;
    a=mat[0][0]; b=mat[0][1]; c=mat[0][2]; d=mat[0][3];
    e=mat[1][0]; f=mat[1][1]; g=mat[1][2]; h=mat[1][3];
    i=mat[2][0]; j=mat[2][1]; k=mat[2][2]; l=mat[2][3];
    m=mat[3][0]; n=mat[3][1]; o=mat[3][2]; p=mat[3][3];

    double q=a*f*k*p - a*f*l*o - a*j*g*p + a*j*h*o + a*n*g*l - a*n*h*k
        - e*b*k*p + e*b*l*o + e*j*c*p - e*j*d*o - e*n*c*l + e*n*d*k
        + i*b*g*p - i*b*h*o - i*f*c*p + i*f*d*o + i*n*c*h - i*n*d*g
        - m*b*g*l + m*b*h*k + m*f*c*l - m*f*d*k - m*j*c*h + m*j*d*g;

    if (SCICore::Math::Abs(q)<0.000000001) {
        imat[0][0]=imat[1][1]=imat[2][2]=imat[3][3]=1;
        imat[1][0]=imat[1][2]=imat[1][3]=0;
        imat[2][0]=imat[2][1]=imat[2][3]=0;
        imat[3][0]=imat[3][1]=imat[3][2]=0;
        cerr << "ERROR - matrix is singular!!!\n";
        return;
    }
    imat[0][0]=(f*k*p - f*l*o - j*g*p + j*h*o + n*g*l - n*h*k)/q;
    imat[0][1]=-(b*k*p - b*l*o - j*c*p + j*d*o + n*c*l - n*d*k)/q;
    imat[0][2]=(b*g*p - b*h*o - f*c*p + f*d*o + n*c*h - n*d*g)/q;
    imat[0][3]=-(b*g*l - b*h*k - f*c*l + f*d*k + j*c*h - j*d*g)/q;

    imat[1][0]=-(e*k*p - e*l*o - i*g*p + i*h*o + m*g*l - m*h*k)/q;
    imat[1][1]=(a*k*p - a*l*o - i*c*p + i*d*o + m*c*l - m*d*k)/q;
    imat[1][2]=-(a*g*p - a*h*o - e*c*p + e*d*o + m*c*h - m*d*g)/q;
    imat[1][3]=(a*g*l - a*h*k - e*c*l + e*d*k + i*c*h - i*d*g)/q;

    imat[2][0]=(e*j*p - e*l*n - i*f*p + i*h*n + m*f*l - m*h*j)/q;
    imat[2][1]=-(a*j*p - a*l*n - i*b*p + i*d*n + m*b*l - m*d*j)/q;
    imat[2][2]=(a*f*p - a*h*n - e*b*p + e*d*n + m*b*h - m*d*f)/q;
    imat[2][3]=-(a*f*l - a*h*j - e*b*l + e*d*j + i*b*h - i*d*f)/q;

    imat[3][0]=-(e*j*o - e*k*n - i*f*o + i*g*n + m*f*k - m*g*j)/q;
    imat[3][1]=(a*j*o - a*k*n - i*b*o + i*c*n + m*b*k - m*c*j)/q;
    imat[3][2]=-(a*f*o - a*g*n - e*b*o + e*c*n + m*b*g - m*c*f)/q;
    imat[3][3]=(a*f*k - a*g*j - e*b*k + e*c*j + i*b*g - i*c*f)/q;
}

void Transform::post_mulmat(double mmat[4][4])
{
    double newmat[4][4];
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    newmat[i][j]=0.0;
	    for(int k=0;k<4;k++){
		newmat[i][j]+=mat[i][k]*mmat[k][j];
	    }
	}
    }
    install_mat(newmat);
}

void Transform::pre_mulmat(double mmat[4][4])
{
    double newmat[4][4];
    for(int i=0;i<4;i++){
	for(int j=0;j<4;j++){
	    newmat[i][j]=0.0;
	    for(int k=0;k<4;k++){
		newmat[i][j]+=mmat[i][k]*mat[k][j];
	    }
	}
    }
    install_mat(newmat);
}

void Transform::perspective(const Point& eyep, const Point& lookat,
			    const Vector& up, double fov,
			    double znear, double zfar,
			    int xres, int yres)
{
    Vector lookdir(lookat-eyep);
    Vector z(lookdir);
    z.normalize();
    Vector x(Cross(z, up));
    x.normalize();
    Vector y(Cross(x, z));
    double xviewsize=Tan(DtoR(fov/2.))*2.;
    double yviewsize=xviewsize*yres/xres;
    double zscale=-znear;
    double xscale=xviewsize*0.5;
    double yscale=yviewsize*0.5;
    x*=xscale;
    y*=yscale;
    z*=zscale;
//    pre_translate(Point(0,0,0)-eyep);
    double m[4][4];
    // Viewing...
    m[0][0]=x.x(); m[0][1]=y.x(); m[0][2]=z.x(); m[0][3]=eyep.x();
    m[1][0]=x.y(); m[1][1]=y.y(); m[1][2]=z.y(); m[1][3]=eyep.y();
    m[2][0]=x.z(); m[2][1]=y.z(); m[2][2]=z.z(); m[2][3]=eyep.z();
    m[3][0]=0;     m[3][1]=0; m[3][2]=0.0;   m[3][3]=1.0;
    invmat(m);
    pre_mulmat(m);
    
    // Perspective...
    m[0][0]=1.0; m[0][1]=0.0; m[0][2]=0.0; m[0][3]=0.0;
    m[1][0]=0.0; m[1][1]=1.0; m[1][2]=0.0; m[1][3]=0.0;
    m[2][0]=0.0; m[2][1]=0.0; m[2][2]=-(zfar-1)/(1+zfar); m[2][3]=-2*zfar/(1+zfar);
    m[3][0]=0.0; m[3][1]=0.0; m[3][2]=-1.0; m[3][3]=0.0;
    pre_mulmat(m);

    pre_scale(Vector(1,-1,1)); // X starts at the top...
    pre_translate(Vector(1,1,0));
    pre_scale(Vector(xres/2., yres/2., 1.0));	
    m[3][3]+=1.0; // hack
}

void Transform::invmat(double m[4][4])
{
  using SCICore::Math::Abs;

    double imat[4][4];
    int i;
    for(i=0;i<4;i++){
        for(int j=0;j<4;j++){
            imat[i][j]=0.0;
        }
        imat[i][i]=1.0;
    }

    // Gauss-Jordan with partial pivoting
    for(i=0;i<4;i++){
        double max=Abs(m[i][i]);
        int row=i;
	int j;
        for(j=i+i;j<4;j++){
            if(Abs(m[j][i]) > max){
                max=Abs(m[j][i]);
                row=j;
            }
        }
        ASSERT(max!=0);
        if(row!=i){
            switch_rows(m, i, row);
            switch_rows(imat, i, row);
        }
        double denom=1./m[i][i];
        for(j=i+1;j<4;j++){
            double factor=m[j][i]*denom;
            sub_rows(m, j, i, factor);
            sub_rows(imat, j, i, factor);
        }
    }

    // Jordan
    for(i=1;i<4;i++){
        ASSERT(m[i][i]!=0);
        double denom=1./m[i][i];
        for(int j=0;j<i;j++){
            double factor=m[j][i]*denom;
            sub_rows(m, j, i, factor);
            sub_rows(imat, j, i, factor);
        }
    }

    // Normalize
    for(i=0;i<4;i++){
        ASSERT(m[i][i]!=0);
        double factor=1./m[i][i];
        for(int j=0;j<4;j++){
            imat[i][j] *= factor;
	    m[i][j]=imat[i][j];
	}
    }
}

void Transform::switch_rows(double m[4][4], int r1, int r2) const
{
    for(int i=0;i<4;i++){
        double tmp=m[r1][i];
        m[r1][i]=m[r2][i];
        m[r2][i]=tmp;
    }
}


void Transform::sub_rows(double m[4][4], int r1, int r2, double mul) const
{
    for(int i=0;i<4;i++)
        m[r1][i] -= m[r2][i]*mul;
}

Transform& Transform::operator=(const Transform& copy)
{
    for(int i=0;i<4;i++)
        for(int j=0;j<4;j++)
            mat[i][j]=copy.mat[i][j];
    inverse_valid=0;
    return *this;
}

} // End namespace Geometry
} // End namespace SCICore

//
// $Log$
// Revision 1.6  2000/07/27 05:23:23  samsonov
// Added implementation of Transform(const Point&, const Vector&, const Vector&, const Vector&)
//
// Revision 1.5  2000/03/13 05:05:12  dmw
// Added Transform::permute for swapping axes, and fixed compute_imat
//
// Revision 1.4  1999/10/07 02:07:56  sparker
// use standard iostreams and complex type
//
// Revision 1.3  1999/09/04 06:01:52  sparker
// Updates to .h files, to minimize #includes
// removed .icc files (yeah!)
//
// Revision 1.2  1999/08/17 06:39:29  sparker
// Merged in modifications from PSECore to make this the new "blessed"
// version of SCIRun/Uintah.
//
// Revision 1.1  1999/07/27 16:56:57  mcq
// Initial commit
//
// Revision 1.1.1.1  1999/04/24 23:12:27  dav
// Import sources
//
//
