
/*
 *  TCLInit.cc: Initialize TCL stuff..
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   Jul 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <TCL.h>
#include <MemStats.h>
#include <NetworkEditor.h>
#include <ThreadStats.h>


void TCL::initialize()
{
    MemStats* memstats=new MemStats;
    memstats->init_tcl();
    ThreadStats* threadstats=new ThreadStats;
    threadstats->init_tcl();
}
