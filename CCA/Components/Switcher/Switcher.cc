#include <Packages/Uintah/CCA/Components/Switcher/Switcher.h>
#include <Packages/Uintah/CCA/Components/ComponentFactory.h>
#include <Packages/Uintah/CCA/Components/ProblemSpecification/ProblemSpecReader.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/CCA/Ports/ProblemSpecInterface.h>
#include <Packages/Uintah/Core/Grid/SimulationState.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Variables/VarTypes.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/CCA/Ports/SolverInterface.h>
#include <Packages/Uintah/Core/Grid/Variables/SoleVariable.h>
#include <Packages/Uintah/Core/Grid/GridP.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/SimpleMaterial.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>
#include <Core/Malloc/Allocator.h>

using namespace Uintah;

Switcher::Switcher(const ProcessorGroup* myworld, ProblemSpecP& ups, bool doAMR)
  : UintahParallelComponent(myworld)
{
  int num_components = 0;
  d_componentIndex = 0;
  d_switchState = idle;

  SolverInterface* solver = dynamic_cast<SolverInterface*>(getPort("solver"));
  ProblemSpecP sim_block = ups->findBlock("SimulationComponent");
  for (ProblemSpecP child = sim_block->findBlock("subcomponent"); child != 0; 
       child = child->findNextBlock("subcomponent")) {

    num_components++;
    string in("");
    if (!child->get("input_file",in))
      throw ProblemSetupException("Need input file for subcomponent");

    UintahParallelComponent* comp = ComponentFactory::create(child, myworld, doAMR);
    SimulationInterface* sim = dynamic_cast<SimulationInterface*>(comp);
    attachPort("sim", sim);
    attachPort("problem spec", scinew ProblemSpecReader(in));
    comp->attachPort("solver", solver);

  }
  
  d_numComponents = num_components;

  switchLabel = VarLabel::create("switch.bool",
                                 SoleVariable<bool>::getTypeDescription());
}

Switcher::~Switcher()
{
  VarLabel::destroy(switchLabel);
}

void Switcher::problemSetup(const ProblemSpecP& params, GridP& grid,
                            SimulationStateP& sharedState)
{
  d_sim = dynamic_cast<SimulationInterface*>(getPort("sim",d_componentIndex));

  ProblemSpecInterface* psi = 
    dynamic_cast<ProblemSpecInterface*>(getPort("problem spec",d_componentIndex));
  if (psi) {
    ProblemSpecP ups = psi->readInputFile();
    d_sim->problemSetup(ups,grid,sharedState);
  } else {
    throw InternalError("psi dynamic_cast failed");
  }
    
  d_sharedState = sharedState;
}
 
void Switcher::scheduleInitialize(const LevelP& level,
                                  SchedulerP& sched)
{
  d_sim->scheduleInitialize(level,sched);
}
 
void Switcher::scheduleComputeStableTimestep(const LevelP& level,
                                             SchedulerP& sched)
{
  cout << "Switcher::scheduleComputeStableTimestep" << endl;
  d_sim->scheduleComputeStableTimestep(level,sched);
}

void
Switcher::scheduleTimeAdvance(const LevelP& level, SchedulerP& sched,
                              int a, int b )
{
  cout << "d_sim = " << d_sim << endl;
  d_sim->scheduleTimeAdvance(level,sched,a,b);
  scheduleSwitchTest(level,sched);
}

void Switcher::scheduleSwitchTest(const LevelP& level, SchedulerP& sched)
{

  d_sim->scheduleSwitchTest(level,sched); // generates switch test data;

  Task* t = scinew Task("Switcher::switchTest",
                        this, & Switcher::switchTest);

  // requires switch_test data;
  t->requires(Task::NewDW,switchLabel);
  sched->addTask(t,level->eachPatch(),d_sharedState->allMaterials());
  if (d_switchState == switching) {
    cout << "RUNNING computeStableTimestep" << endl;
    //d_sim->scheduleComputeStableTimestep(level,sched);
  }
}

void Switcher::switchTest(const ProcessorGroup*,
                          const PatchSubset* patches,
                          const MaterialSubset* matls,
                          DataWarehouse* old_dw, DataWarehouse* new_dw)
{

  SoleVariable<bool> switch_condition;
  new_dw->get(switch_condition,switchLabel,getLevel(patches));
  cout << "switch_condition = " << switch_condition << endl;

  if (switch_condition) {
    d_switchState = switching;
  } else
    d_switchState = idle;

}


bool Switcher::needRecompile(double time, double delt, const GridP& grid)
{
  cout << "In needRecompile, returning " << (d_switchState == switching) << endl;
  bool retval = false;
  if (d_switchState == switching) {
    d_componentIndex++;
    d_sharedState->clearMaterials();
    d_sharedState->d_switchState = true;
    d_sim = 
      dynamic_cast<SimulationInterface*>(getPort("sim",d_componentIndex)); 
    ProblemSpecInterface* psi = 
      dynamic_cast<ProblemSpecInterface*>(getPort("problem spec",
                                                  d_componentIndex));
    if (psi) {
      ProblemSpecP ups = psi->readInputFile();
      d_sim->problemSetup(ups,const_cast<GridP&>(grid),d_sharedState);
    }
    d_sharedState->finalizeMaterials();
    retval = true;
  } else
    d_sharedState->d_switchState = false;
  retval |= d_sim->needRecompile(time, delt, grid);
  return retval;
}

void Switcher::addToTimestepXML(ProblemSpecP& spec)
{
  spec->appendElement("switcherComponentIndex", (int) d_componentIndex, true);
  spec->appendElement("switcherState", (int) d_switchState, true);
}

void Switcher::readFromTimestepXML(const ProblemSpecP& spec)
{
  // problemSpec doesn't handle unsigned
  ProblemSpecP ps = (ProblemSpecP) spec;
  int tmp;
  ps->get("switcherComponentIndex", tmp);
  d_componentIndex = tmp; 
  ps->get("switcherState", tmp);
  d_switchState = (switchState) tmp;
  cout << "  RESTART index = " << d_componentIndex << " STATE = " << d_switchState << endl;
}
