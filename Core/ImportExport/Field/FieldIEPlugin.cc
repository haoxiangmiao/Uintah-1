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
 *  Persistent.h: Base class for persistent objects...
 *
 *  Written by:
 *   Michael Callahan
 *   Department of Computer Science
 *   University of Utah
 *   May 2004
 *
 *  Copyright (C) 2004 SCI Institute
 */

#include <Core/ImportExport/Field/FieldIEPlugin.h>
#include <Core/Containers/StringUtil.h>

#include <sgi_stl_warnings_off.h>
#include <map>
#include <sgi_stl_warnings_on.h>

using namespace std;

#define DEBUG 0

namespace SCIRun {

map<string, FieldIEPlugin *> *FieldIEPlugin::table = 0;

#ifdef __APPLE__
  // On the Mac, this comes from Core/Util/DynamicLoader.cc because
  // the constructor will actually fire from there.  When it is declared
  // in this file, it does not "construct" and thus causes seg faults.
  // (Yes, this is a hack.  Perhaps this problem will go away in later
  // OSX releases, but probably not as it has something to do with the
  // Mac philosophy on when to load dynamic libraries.)
  extern Mutex fieldIEPluginMutex;
#else
  // Same problem on Linux really.  We need control of the static
  // initializer order.
  extern Mutex fieldIEPluginMutex;
#endif


//----------------------------------------------------------------------
FieldIEPlugin::FieldIEPlugin(const string& pname,
			     const string& fextension,
			     const string& fmagic,
			     FieldHandle (*freader)(ProgressReporter *pr,
						    const char *filename),
			     void (*fwriter)(ProgressReporter *pr,
					     FieldHandle f,
					     const char *filename))
  : pluginname(pname),
    fileextension(fextension),
    filemagic(fmagic),
    filereader(freader),
    filewriter(fwriter)
{
  fieldIEPluginMutex.lock();
  if (!table)
  {
    table = scinew map<string, FieldIEPlugin *>();
  }

  string tmppname = pluginname;
  int counter = 2;
  while (1)
  {
    map<string, FieldIEPlugin *>::iterator loc = table->find(tmppname);
    if (loc == table->end())
    {
      if (tmppname != pluginname) { ((string)pluginname) = tmppname; }
      (*table)[pluginname] = this;
      break;
    }
    if (*(*loc).second == *this)
    {
      cerr << "WARNING: FieldIEPlugin '" << tmppname << "' duplicated.\n";
      break;
    }

    cout << "WARNING: Multiple FieldIEPlugins with '" << pluginname
	 << "' name.\n";
    tmppname = pluginname + "(" + to_string(counter) + ")";
    counter++;
  }

  fieldIEPluginMutex.unlock();
}



FieldIEPlugin::~FieldIEPlugin()
{
  if (table == NULL)
  {
    cerr << "WARNING: FieldIEPlugin.cc: ~FieldIEPlugin(): table is NULL\n";
    cerr << "         For: " << pluginname << "\n";
    return;
  }

  fieldIEPluginMutex.lock();

  map<string, FieldIEPlugin *>::iterator iter = table->find(pluginname);
  if (iter == table->end())
  {
    cerr << "WARNING: FieldIEPlugin " << pluginname << 
      " not found in database for removal.\n";
  }
  else
  {
    table->erase(iter);
  }

  if (table->size() == 0)
  {
    delete table;
    table = 0;
  }

  fieldIEPluginMutex.unlock();
}


bool
FieldIEPlugin::operator==(const FieldIEPlugin &other) const
{
  return (pluginname == other.pluginname &&
	  fileextension == other.fileextension &&
	  filemagic == other.filemagic &&
	  filereader == other.filereader &&
	  filewriter == other.filewriter);
}

} // End namespace SCIRun


