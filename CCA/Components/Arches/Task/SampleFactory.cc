#include <CCA/Components/Arches/Task/SampleFactory.h>
#include <CCA/Components/Arches/Task/SampleTask.h>
#include <CCA/Components/Arches/Task/TemplatedSampleTask.h>
#include <CCA/Components/Arches/Task/TaskInterface.h>

using namespace Uintah; 

SampleFactory::SampleFactory()
{}

SampleFactory::~SampleFactory()
{}

void 
SampleFactory::register_all_tasks( ProblemSpecP& db )
{ 

  //This is meant to be the interface to the inputfile for the various tasks
  //associated with this factory. 

  //The sample task: 
  std::string tname = "sample_task"; 
  TaskInterface::TaskBuilder* sample_builder = scinew SampleTask::Builder(tname,0); 
  register_task(tname, sample_builder); 

  _active_tasks.push_back(tname); 

  //The templated task: 
  tname = "templated_task";
  TaskInterface::TaskBuilder* templated_sample_builder = 
    scinew TemplatedSampleTask<SpatialOps::SVolField>::Builder(tname,0); 
  register_task(tname, templated_sample_builder); 

  _active_tasks.push_back(tname); 

}

void 
SampleFactory::build_all_tasks( ProblemSpecP& db )
{ 

  typedef std::vector<std::string> SV; 

  for ( SV::iterator i = _active_tasks.begin(); i != _active_tasks.end(); i++){ 

    TaskInterface* tsk = retrieve_task(*i); 
    tsk->problemSetup( db ); 

    tsk->create_local_labels(); 

  }


}