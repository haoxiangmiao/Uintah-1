
/*
 *  SegFldOps.cc:  Unfinished modules
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Packages/DaveW/Core/Datatypes/General/SegFld.h>
#include <Packages/DaveW/Core/Datatypes/General/SegFldPort.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Core/Datatypes/ScalarFieldRGchar.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLvar.h>
#include <iostream>
using std::cerr;

namespace DaveW {
using namespace DaveW;
using namespace SCIRun;

class SegFldOps : public Module {
    ScalarFieldIPort* iscl;
    ScalarFieldOPort* oscl;
    ScalarFieldOPort* obit;
    SegFldIPort* iseg;
    SegFldOPort* oseg;
public:
    SegFldHandle segFldHandle;
    int gen;
    clString lastType;
    int tcl_exec;
    TCLstring itype;
    TCLstring meth;
    TCLint sendCharFlag;
    TCLint annexSize;

    void tcl_command( TCLArgs&, void * );
    SegFldOps(const clString& id);
    virtual ~SegFldOps();
    virtual void execute();
};

extern "C" Module* make_SegFldOps(const clString& id)
{
    return new SegFldOps(id);
}

SegFldOps::SegFldOps(const clString& id)
: Module("SegFldOps", id, Filter), itype("itype", id, this), 
  meth("meth", id, this), sendCharFlag("sendCharFlag", id, this),
  annexSize("annexSize", id, this)
{
    iscl=new ScalarFieldIPort(this, "ScalarIn", ScalarFieldIPort::Atomic);
    add_iport(iscl);
    iseg=new SegFldIPort(this, "SegIn", SegFldIPort::Atomic);
    add_iport(iseg);
    // Create the output port
    oscl=new ScalarFieldOPort(this, "CharOut", ScalarFieldIPort::Atomic);
    add_oport(oscl);
    obit=new ScalarFieldOPort(this, "BitOut", ScalarFieldIPort::Atomic);
    add_oport(obit);
    oseg=new SegFldOPort(this, "SegOut", SegFldIPort::Atomic);
    add_oport(oseg);
    gen=-1;
    lastType="empty";
}

SegFldOps::~SegFldOps()
{
}

void SegFldOps::execute()
{
    if (itype.get()=="char") {
	if (lastType != "char") gen=-1;
	ScalarFieldHandle iSFHandle;
	if(!iscl->get(iSFHandle))
	    return;
	if (!iSFHandle.get_rep()) {
	    cerr << "Error: empty ScalarFieldHandle!\n";
	    return;
	}
	ScalarFieldRGBase* sfb=iSFHandle->getRGBase();
	if (!sfb) {
	    cerr << "Bad input -- not an RGBase\n";
	    return;
	}
	ScalarFieldRGchar* sf=sfb->getRGChar();
	if (!sf) {
	    cerr << "Error: Bad input -- not an RGchar\n";
	    return;
	}
	if (sf->generation != gen) 
	    segFldHandle=scinew SegFld(sf);
	gen=sf->generation;
	itype.set("oldseg");
	tcl_exec=1;
    } else if (itype.get()=="newseg") {
	SegFldHandle iSFHandle;
	if (!iseg->get(iSFHandle))
	    return;
	if (!iSFHandle.get_rep()) {
	    cerr << "Error: empty SegFldHandle!\n";
	    return;
	}
	segFldHandle=iSFHandle;
	lastType=itype.get();
	itype.set("oldseg");
	tcl_exec=1;
    } else if (itype.get()=="oldseg") {
	if (!segFldHandle.get_rep()) {
	    cerr << "Error: empty SegFldHandle!\n";
	    return;
	}
    }

    lastType=itype.get();

    if (tcl_exec) {
	if (meth.get()=="annex") {
	    int ann=annexSize.get();
	    if (ann>0) {
		segFldHandle->killSmallCore/CCA/Components(ann);
	    }
	}
	tcl_exec=0;
    }

    oseg->send(segFldHandle);
    if (sendCharFlag.get()) {
	ScalarFieldRGchar* sfrgc=segFldHandle->getTypeFld();
	ScalarFieldHandle sfh(sfrgc);
	oscl->send(sfh);
	ScalarFieldRG* sf=segFldHandle->getBitFld();
	ScalarFieldHandle s(sf);
	obit->send(s);
    }
}

void SegFldOps::tcl_command(TCLArgs& args, void* userdata) {
    if (args[1] == "tcl_exec") {
	tcl_exec=1;
	want_to_execute();
    } else if (args[1] == "audit") {
        if (segFldHandle.get_rep()) segFldHandle->audit();
	else cerr << "Can't audit -- I don't have a segFld yet.\n";
    } else if (args[1] == "print") {
	if (segFldHandle.get_rep()) segFldHandle->printCore/CCA/Components();
	else cerr << "Can't print -- I don't have a segFld yet.\n";
    } else if (args[1] == "compress") {
	if (segFldHandle.get_rep()) segFldHandle->compress();
	else cerr << "Can't print -- I don't have a segFld yet.\n";
    } else {
        Module::tcl_command(args, userdata);
    }
}
} // End namespace DaveW



