

#ifndef FrameworkImpl_h
#define FrameworkImpl_h

#include <map>
#include <testprograms/Component/framework/cca_sidl.h>


namespace sci_cca {

class ComponentRecord;
class UsePortRecord;
class ProvidePortRecord;
class Registry;

class ConnectionServicesImpl;

class FrameworkImpl : public Framework_interface {
private:
  string hostname_;
  ComponentID id_;
  Registry *registry_;
  map<string, Port> ports_;

  typedef map<string, Port>::iterator port_iterator;

public:
  FrameworkImpl();
  virtual ~FrameworkImpl();
  
  virtual bool registerComponent( const string &, const string &, Component &);
  virtual void unregisterComponent( const ComponentID & );

  virtual Port getPort( const ComponentID &, const string &);
  virtual void registerUsesPort( const ComponentID &, const PortInfo &);
  virtual void unregisterUsesPort( const ComponentID &, const string & );
  virtual void addProvidesPort( const ComponentID &, const Port &,
				const PortInfo&);
  virtual void removeProvidesPort( const ComponentID &, const string &);
  virtual void releasePort( const ComponentID &, const string &);

  friend ConnectionServicesImpl;
};

} // namespace sci_cca

#endif FrameworkImpl_h
