#include <Core/Thread/Mutex.h>
#include <Core/Thread/AtomicCounter.h>
#include <testprograms/Component/framework/ComponentImpl.h>

#include <sstream>
#include <iostream>

namespace sci_cca {

using std::cerr;

ComponentImpl::ComponentImpl()
{
}

ComponentImpl::~ComponentImpl()
{
  cerr << "Component destructor\n";
}

void 
ComponentImpl::setServices( const Services &s )
{
  if ( s ) {
    services_ = pidl_cast<SciServices>(s);
  }
  else {
    // Component shutdown
    services_->shutdown();
    cerr << "component shutdown\n";
  }
}

} // namespace sci_cca

