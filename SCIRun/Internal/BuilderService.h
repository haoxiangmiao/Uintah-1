/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


/*
 *  BuilderService.h: Implementation of the CCA BuilderService interface for SCIRun
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   October 2001
 *
 */

#ifndef SCIRun_BuilderService_h
#define SCIRun_BuilderService_h

#include <Core/CCA/spec/cca_sidl.h>
#include <SCIRun/Internal/InternalComponentModel.h>
#include <SCIRun/Internal/InternalComponentInstance.h>
#include <SCIRun/Bridge/AutoBridge.h>

namespace SCIRun {
  class SCIRunFramework;
  class BuilderService : public sci::cca::ports::BuilderService, public InternalComponentInstance {
  public:
    virtual ~BuilderService();

    static InternalComponentInstance* create(SCIRunFramework* framework,
					     const std::string& name);


    virtual sci::cca::ComponentID::pointer createInstance(const std::string& instanceName,
						 const std::string& className,
						 const sci::cca::TypeMap::pointer& properties);
    virtual SSIDL::array1<sci::cca::ComponentID::pointer> getComponentIDs();
    virtual sci::cca::TypeMap::pointer getComponentProperties(const sci::cca::ComponentID::pointer& cid);
    virtual void setComponentProperties(const sci::cca::ComponentID::pointer& cid,
					const sci::cca::TypeMap::pointer& map);
    virtual sci::cca::ComponentID::pointer getDeserialization(const std::string& s);
    virtual sci::cca::ComponentID::pointer getComponentID(const std::string& componentInstanceName);
    virtual void destroyInstance(const sci::cca::ComponentID::pointer& toDie,
				 float timeout);
    virtual SSIDL::array1<std::string> getProvidedPortNames(const sci::cca::ComponentID::pointer& cid);
    virtual SSIDL::array1<std::string> getUsedPortNames(const sci::cca::ComponentID::pointer& cid);
    virtual sci::cca::TypeMap::pointer getPortProperties(const sci::cca::ComponentID::pointer& cid,
						const std::string& portname);
    virtual void setPortProperties(const sci::cca::ComponentID::pointer& cid,
				   const std::string& portname,
				   const sci::cca::TypeMap::pointer& map);
    virtual sci::cca::ConnectionID::pointer connect(const sci::cca::ComponentID::pointer& user,
					   const std::string& usingPortName,
					   const sci::cca::ComponentID::pointer& provider,
					   const ::std::string& providingPortName);
    virtual SSIDL::array1<sci::cca::ConnectionID::pointer> getConnectionIDs(const SSIDL::array1<sci::cca::ComponentID::pointer>& componentList);
    virtual sci::cca::TypeMap::pointer getConnectionProperties(const sci::cca::ConnectionID::pointer& connID);
    virtual void setConnectionProperties(const sci::cca::ConnectionID::pointer& connID,
					 const sci::cca::TypeMap::pointer& map);
    virtual void disconnect(const sci::cca::ConnectionID::pointer& connID,
			    float timeout);
    virtual void disconnectAll(const sci::cca::ComponentID::pointer& id1,
			       const sci::cca::ComponentID::pointer& id2,
			       float timeout);

    virtual SSIDL::array1<std::string> getCompatiblePortList(
		const sci::cca::ComponentID::pointer& c1,
		const std::string& port1,
		const sci::cca::ComponentID::pointer& c2);
    
    //Bridge methods 
    virtual SSIDL::array1<std::string> getBridgablePortList(
	        const sci::cca::ComponentID::pointer& c1,
	        const std::string& port1,
	        const sci::cca::ComponentID::pointer& c2);
    virtual std::string generateBridge(const sci::cca::ComponentID::pointer& c1,
				       const string& port1,
				       const sci::cca::ComponentID::pointer& c2,
				       const string& port2);

    sci::cca::Port::pointer getService(const std::string&);

    int addComponentClasses(const std::string &loaderName);
    int removeComponentClasses(const std::string &loaderName);

    int addLoader(const std::string &loaderName, const std::string &user, const std::string &domain, const std::string &loaderPath );
    int removeLoader(const std::string &name);
    //virtual void registerFramework(const std::string &frameworkURL); 
    //virtual void registerServices(const sci::cca::Services::pointer &svc);


  private:
    BuilderService(SCIRunFramework* fwk, const std::string& name);
    std::vector<sci::cca::Services::pointer> servicesList;	
    std::string getFrameworkURL();
    AutoBridge autobr;
  };
}

#endif
