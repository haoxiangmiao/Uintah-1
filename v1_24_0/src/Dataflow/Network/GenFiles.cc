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


/* GenFiles.cc */

#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <string.h>
#include <Dataflow/Network/ComponentNode.h>
#include <Dataflow/Network/SkeletonFiles.h>
#include <Dataflow/Network/GenFiles.h>
#include <Dataflow/Network/FileUtils.h>
#include <Core/Util/Environment.h>

#define PERM S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH

#define DEBUG 0

namespace SCIRun {

using std::string;

/*! these functions all assume that the files and/or directories
    about to be generated, do not already exist */ 

int
GenPackage(char* package, char* psepath)
{
#if DEBUG
  printf ("GenPack\n");
#endif
  int check=0,checkall=0;
  char* strbuf = 0;
  FILE* file = 0;

  bool pse_core_dir = true;
  string packstring("");
  if (strcmp(package,"SCIRun"))
    {
      pse_core_dir = false;
      packstring = string("Packages/") + package + string("/");
    }
  const char *packdir = packstring.c_str();


  /* allocate enough space to hold the largest path */
  strbuf = new char[strlen(packdir)+strlen(psepath)+50];

  /* create all directories associated with a package */
  sprintf(strbuf,"%s/src/%s",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);

  sprintf(strbuf,"%s/src/%sDataflow",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);

  sprintf(strbuf,"%s/src/%sCore",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);

  sprintf(strbuf,"%s/src/%sDataflow/Modules",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);
  
  sprintf(strbuf,"%s/src/%sDataflow/GUI",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);

  sprintf(strbuf,"%s/src/%sDataflow/XML",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);

#if 0
  sprintf(strbuf,"%s/src/%sshare",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);
#endif

  sprintf(strbuf,"%s/src/%sCore/Datatypes",psepath,packdir);
  checkall |= check = mkdir(strbuf,PERM);
  if (check)
    printf("could not create directory \"%s\"\n",strbuf);

  if (checkall) {
    printf("Could not create one or more directories.  Giving up.");
    return 0;
  }

#if 0
  /* create all the non-directory files associated with a package */
  sprintf(strbuf,"%s/src/%sshare/share.h",psepath,package);
  file = fopen(strbuf,"w");
  fprintf(file,share_skeleton,package,package,package,
	  package,package);
  fclose(file);

  sprintf(strbuf,"%s/src/%sshare/DllEntry.cc",psepath,packdir);
  file = fopen(strbuf,"w");
  fprintf(file,dllentry_skeleton,package);
  fclose(file);
#endif

  if (!pse_core_dir)
    {
      sprintf(strbuf,"%s/src/%ssub.mk",psepath,packdir);
      file = fopen(strbuf,"w");
      fprintf(file,package_submk_skeleton,package);
      fclose(file);
    }

  sprintf(strbuf,"%s/src/%sDataflow/sub.mk",
	  psepath,packdir);
  file = fopen(strbuf,"w");
  fprintf(file,dataflow_submk_skeleton,packdir);
  fclose(file);

  sprintf(strbuf,"%s/src/%sCore/sub.mk",
	  psepath,packdir);
  file = fopen(strbuf,"w");
  fprintf(file,core_submk_skeleton,packdir);
  fclose(file);

  sprintf(strbuf,"%s/src/%sDataflow/Modules/sub.mk",
	  psepath,packdir);
  file = fopen(strbuf,"w");
  fprintf(file,modules_submk_skeleton,packdir);
  fclose(file);

  sprintf(strbuf,"%s/src/%sCore/Datatypes/sub.mk",psepath,packdir);
  file = fopen(strbuf,"w");
  fprintf(file,datatypes_submk_skeleton,packdir);
  fclose(file);

  sprintf(strbuf,"%s/src/%sDataflow/GUI/sub.mk",psepath,packdir);
  file = fopen(strbuf,"w");
  fprintf(file,gui_submk_skeleton,packdir,packdir);
  fclose(file);

  delete[] strbuf;

  return 1;
}

int GenCategory(char* catname, char* package, char* psepath)
{
#if DEBUG
  printf ("GenCat\n");
#endif
  int check;
  char* strbuf = 0;
  FILE* file = 0;

  string packstring("");
  if (strcmp(package,"SCIRun"))
    packstring = string("Packages/") + package + string("/");
  const char *packdir = packstring.c_str();


  strbuf = new char[strlen(packdir)+strlen(psepath)+
		    strlen(catname)+50];

  /* create category directory */
  sprintf(strbuf,"%s/src/%sDataflow/Modules/%s",
	  psepath,packdir,catname);
  check = mkdir(strbuf,PERM);
  if (check) {
    printf("could not create directory \"%s\".  Giving up.\n",strbuf);
    return 0;
  }

  /* create category sub.mk file */
  sprintf(strbuf,"%s/src/%sDataflow/Modules/%s/sub.mk",
	  psepath,packdir,catname);
  file = fopen(strbuf,"w");
  fprintf(file,category_submk_skeleton,packdir,catname);
  fclose(file);

  /* edit the modules sub.mk file - add the new category */

  char* modname = new char[strlen(catname)+50];
  sprintf(modname,"\t$(SRCDIR)/%s\\\n",catname);
  strbuf = new char[strlen(psepath)+strlen(packdir)+50];
  sprintf(strbuf,"%s/src/%sDataflow/Modules/sub.mk",
	  psepath,packdir);
  InsertStringInFile(strbuf,"#[INSERT NEW CATEGORY DIR HERE]",modname);
  delete[] strbuf;

  return 1;
}

int
GenComponent(component_node* n, char* package, char* psepath)
{
  printf ("GenComp\n");
  
  char* filename = 0;
  char* strbuf = 0;
  FILE* file = 0;
  int length;

  string packstring("");
  if (strcmp(package,"SCIRun"))
    packstring = string("Packages/") + package + string("/");
  const char *packdir = packstring.c_str();

  /* generate a skeleton .cc file */
  length = strlen(n->name)+strlen(psepath)+
    strlen(packdir)+strlen(n->category)+50;
  filename = new char[length];
  sprintf(filename,"%s/src/%sDataflow/Modules/%s/%s.cc",
	  psepath,packdir,n->category,n->name);
  file = fopen(filename,"w");
  fprintf(file,component_skeleton,n->name,
	  sci_getenv("USER"),"TODAY'S DATE HERE",
	  package,
	  n->name,n->name,n->name,
	  n->name,n->name,n->name,n->name,
	  n->category,package,n->name,n->name,
	  n->name,n->name,package);
  fclose(file);
  delete[] filename;

  /* generate a full component .xml file */
  length = strlen(n->name)+strlen(psepath)+
    strlen(packdir)+50;
  filename = new char[length];
  sprintf(filename,"%s/src/%sDataflow/XML/%s.xml",psepath,
	  packdir,n->name);
  WriteComponentNodeToFile(n,filename);
  delete[] filename;

  if (n->gui->parameters->size()) {
    /* generate a skeleton .tcl file */
    length = strlen(n->name)+strlen(psepath)+
      strlen(packdir)+50;
    filename = new char[length];
    sprintf(filename,"%s/src/%sDataflow/GUI/%s.tcl",psepath,
	    packdir,n->name);
    file = fopen(filename,"w");
    fprintf(file,gui_skeleton,package,n->category,n->name,n->name,filename);
    fclose(file);
    delete[] filename;
  }

  /* edit the category sub.mk file - add the new component */
  char* modname = new char[strlen(n->name)+50];
  sprintf(modname,"\t$(SRCDIR)/%s.cc\\\n",n->name);
  strbuf = new char[strlen(psepath)+strlen(packdir)+strlen(n->category)+50];
  sprintf(strbuf,"%s/src/%sDataflow/Modules/%s/sub.mk",
	  psepath,packdir,n->category);
  InsertStringInFile(strbuf,"#[INSERT NEW CODE FILE HERE]",modname);
  delete[] strbuf;
  delete[] modname;

  if (n->gui->parameters->size()) {
    /* edit the GUI sub.mk file - add the new component */
    modname = new char[strlen(n->name)+50];
    sprintf(modname,"\t$(SRCDIR)/%s.tcl\\\n",n->name);
    strbuf = new char[strlen(psepath)+strlen(packdir)+strlen(n->category)+50];
    sprintf(strbuf,"%s/src/%sDataflow/GUI/sub.mk",psepath,packdir);
    InsertStringInFile(strbuf,"#[INSERT NEW TCL FILE HERE]",modname);
    delete[] strbuf;
  }

  return 1;
}

} // End namespace SCIRun



