#include <CCA/Components/SwitchingCriteria/None.h>
#include <Core/Grid/Task.h>
#include <Core/Grid/Variables/VarTypes.h>
#include <CCA/Ports/Scheduler.h>

using namespace Uintah;

None::None()
{
}

None::~None()
{
}

void None::problemSetup(const ProblemSpecP& ps, 
                        const ProblemSpecP& restart_prob_spec, 
                        SimulationStateP& state)
{
  d_sharedState = state;
}

void None::scheduleSwitchTest(const LevelP& level, SchedulerP& sched)
{
  Task* t = scinew Task("switchTest", this, &None::switchTest);

  t->computes(d_sharedState->get_switch_label(), level.get_rep());
  sched->addTask(t, level->eachPatch(),d_sharedState->allMaterials());
}


void None::switchTest(const ProcessorGroup* group,
                      const PatchSubset* patches,
                      const MaterialSubset* matls,
                      DataWarehouse* old_dw,
                      DataWarehouse* new_dw)
{
  double sw = 0;
  max_vartype switch_condition(sw);
  new_dw->put(switch_condition,d_sharedState->get_switch_label(),getLevel(patches));
}
