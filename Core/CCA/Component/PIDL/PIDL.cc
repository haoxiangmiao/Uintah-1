/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/


/*
 *  PIDL.h: Include a bunch of PIDL files for external clients
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1999
 *
 *  Copyright (C) 1999 SCI Group
 */

#include <Core/CCA/Component/PIDL/PIDL.h>
#include <Core/CCA/Component/PIDL/Object_proxy.h>
#include <Core/CCA/Component/PIDL/Warehouse.h>
#include <Core/CCA/Component/Comm/SocketSpChannel.h>
#include <Core/CCA/Component/Comm/SocketEpChannel.h>
#include <Core/CCA/Component/Comm/SocketMessage.h>
#include <Core/CCA/Component/Comm/Intra/IntraComm.h>
#include <Core/Exceptions/InternalError.h> 
#include <sci_defs.h>
#include <iostream>
#include <sstream>

#ifdef HAVE_MPI
#include <Core/CCA/Component/Comm/Intra/IntraCommMPI.h>
#endif

#ifdef HAVE_GLOBUS
#include <Core/CCA/Component/Comm/NexusSpChannel.h>
#include <Core/CCA/Component/Comm/NexusEpChannel.h>
#include <Core/CCA/Component/Comm/CommNexus.h>
#endif

//Inter-Component Comm libraries supported
#define COMM_SOCKET 1
#define COMM_NEXUS 2

//Intra-Component Comm libraries supported
#define INTRA_COMM_MPI 1

static int comm_type = 0;
static int intra_comm_type = 0;
using namespace SCIRun;

Warehouse* PIDL::warehouse;

void
PIDL::initialize(int, char*[])
{
  //Default for communication purposes 
  //setCommunication(COMM_NEXUS);
  setCommunication(COMM_SOCKET);

  switch (comm_type) {
  case COMM_SOCKET:
    SocketMessage::setSiteTag();
    break;
#ifdef HAVE_GLOBUS
  case COMM_NEXUS:
    CommNexus::initialize();
    break;
#endif

  }

#ifdef HAVE_MPI
  setIntraCommunication(INTRA_COMM_MPI);
#endif

  if(!warehouse){
    warehouse=new Warehouse;
  }
}

void
PIDL::finalize()
{
  switch (comm_type) {
  case COMM_SOCKET:
    break;
#ifdef HAVE_GLOBUS
  case COMM_NEXUS:
    CommNexus::finalize();
    break;
#endif
  }
}

SpChannel*  
PIDL::getSpChannel() {
  switch (comm_type) {
  case COMM_SOCKET:
    return (new SocketSpChannel());
#ifdef HAVE_GLOBUS
  case COMM_NEXUS:
    return (new NexusSpChannel());
#endif
  default:
    return NULL;
  }
}

EpChannel*  
PIDL::getEpChannel() {
  switch (comm_type) {
  case COMM_SOCKET:
    return (new SocketEpChannel());

#ifdef HAVE_GLOBUS
  case COMM_NEXUS:
    return (new NexusEpChannel());
#endif
  default:
    return (new SocketEpChannel());
  }
}

Warehouse*
PIDL::getWarehouse()
{
  if(!warehouse)
    throw SCIRun::InternalError("Warehouse not initialized!\n");
  return warehouse;
}

Object::pointer
PIDL::objectFrom(const URL& url)
{
  return Object::pointer(new Object_proxy(url));
}

Object::pointer 
PIDL::objectFrom(const int urlc, const URL urlv[], int mysize, int myrank)
{
  return Object::pointer(new Object_proxy(urlc,urlv,mysize,myrank));
}

Object::pointer 
PIDL::objectFrom(const std::vector<URL>& urlv, int mysize, int myrank)
{
  return Object::pointer(new Object_proxy(urlv,mysize,myrank));
}

void 
PIDL::serveObjects()
{
  if(!warehouse)
    throw SCIRun::InternalError("Warehouse not initialized!\n");
  warehouse->run();
}

IntraComm* 
PIDL::getIntraComm()
{
#ifdef HAVE_MPI
  switch (intra_comm_type) {
  case (INTRA_COMM_MPI):
    return (new IntraCommMPI());
  default:
    return (new IntraCommMPI());
  }
#endif
  return NULL;
}
 
//PRIVATE:

void
PIDL::setCommunication(int c)
{
  if (comm_type != 0)
    throw SCIRun::InternalError("Cannot modify communication setting after it has been set once\n");
  else { 
    comm_type = c;
  }
}

void
PIDL::setIntraCommunication(int c)
{
  if (intra_comm_type != 0)
    throw SCIRun::InternalError("Cannot modify communication setting after it has been set once\n");
  else {
    intra_comm_type = c;
  }
}

bool
PIDL::isNexus(){
  return comm_type==COMM_NEXUS;
}



