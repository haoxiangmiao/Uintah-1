
#ifndef PLANEDPY_H
#define PLANEDPY_H 1

#include <Core/Geometry/Vector.h>
#include <Core/Thread/Runnable.h>
#include <X11/Xlib.h>

namespace rtrt {

  using namespace SCIRun;
  
class PlaneDpy : public Runnable {
    int xres, yres;
    void move(int x, int y);
public:
    Vector n;
    double d;
    PlaneDpy(const Vector& v, const Point& p);
    virtual ~PlaneDpy();
    virtual void run();
};

} // end namespace rtrt

#endif

