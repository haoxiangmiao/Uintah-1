#ifndef __GEOMETRY_OBJECT_FACTORY_H__
#define __GEOMETRY_OBJECT_FACTORY_H__

// add #include for each ConstitutiveModel here
#include <Uintah/Interface/ProblemSpecP.h>
#include <vector>

namespace Uintah {
namespace Components {
using Uintah::Interface::ProblemSpecP;
class GeometryObject;

class GeometryObjectFactory
{
public:
  // this function has a switch for all known go_types
  // and calls the proper class' readParameters()
  // addMaterial() calls this
  static void create(const ProblemSpecP& ps,
		     std::vector<GeometryObject*>& objs);
};

} // end namespace Components
} // end namespace Uintah


#endif /* __GEOMETRY_OBJECT_FACTORY_H__ */
