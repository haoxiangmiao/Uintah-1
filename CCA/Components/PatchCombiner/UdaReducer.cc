/*
 * The MIT License
 *
 * Copyright (c) 1997-2012 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <CCA/Components/PatchCombiner/UdaReducer.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Grid/Task.h>
#include <Core/Grid/Variables/MaterialSetP.h>
#include <CCA/Ports/Scheduler.h>
#include <CCA/Ports/LoadBalancer.h>
#include <Core/Grid/Material.h>
#include <Core/Grid/Variables/VarTypes.h>
#include <Core/Grid/SimulationState.h>
#include <Core/Grid/SimpleMaterial.h>

#include <iomanip>

using namespace std;
using namespace Uintah;

UdaReducer::UdaReducer(const ProcessorGroup* world, string udaDir)
  : UintahParallelComponent(world), udaDir_(udaDir), dataArchive_(0),
    timeIndex_(0)
{
  delt_label = VarLabel::create("delT", delt_vartype::getTypeDescription());
}

UdaReducer::~UdaReducer()
{
  delete dataArchive_;
  
  VarLabel::destroy(delt_label);
  for (unsigned int i = 0; i < labels_.size(); i++)
    VarLabel::destroy(labels_[i]);
}
//______________________________________________________________________
//
void UdaReducer::problemSetup(const ProblemSpecP& prob_spec, 
                              const ProblemSpecP& restart_ps, 
                              GridP& grid, 
                              SimulationStateP& state)
{
  d_sharedState = state;

  
  dataArchive_ = scinew DataArchive(udaDir_, d_myworld->myrank(), d_myworld->size());
  dataArchive_->queryTimesteps(timesteps_, times_);
  dataArchive_->turnOffXMLCaching();

  // try to add a time to times_ that will get us passed the end of the
  // simulation -- yes this is a hack!!
  times_.push_back(10 * times_[times_.size() - 1]);
  timesteps_.push_back(999999999);

  //oldGrid_ = dataArchive_->queryGrid(times_[0]);
}
//______________________________________________________________________
//
void UdaReducer::scheduleInitialize(const LevelP& level, 
                                    SchedulerP& sched)
{
#if 0
  // labels_ should be empty, but just in case...
  for (unsigned int i = 0; i < labels_.size(); i++){
    VarLabel::destroy(labels_[i]);
  }

  
  labels_.clear();
#endif


  lb = sched->getLoadBalancer();

  vector<string> varNames;
  vector< const TypeDescription *> typeDescriptions;
  
  dataArchive_->queryVariables( varNames, typeDescriptions );
  
  for (unsigned int i = 0; i < varNames.size(); i++) {
    labels_.push_back(VarLabel::create( varNames[i], typeDescriptions[i] ));
  }
  
  //__________________________________
  //
  Task* t = scinew Task("UdaReducer::initialize", this, 
                         &UdaReducer::initialize);
  
  
  MaterialSet* globalMatlSet = scinew MaterialSet();
  globalMatlSet->add(-1);
  
  const MaterialSubset* minusOne_MS = globalMatlSet->getUnion();
  
  const MaterialSet* all_matls = d_sharedState->allMaterials();
  const MaterialSubset* all_matls_MS = all_matls->getUnion();
  
  
  t->computes(delt_label, minusOne_MS);
  sched->addTask(t, level->eachPatch(), globalMatlSet);
}
//______________________________________________________________________
//
void
UdaReducer::scheduleTimeAdvance( const LevelP& level, 
                                 SchedulerP& sched)
{

  GridP grid = level->getGrid();

  const PatchSet* perProcPatches = lb->getPerProcessorPatchSet(grid);

  // so we can tell the task which matls to use (sharedState doesn't have
  // any matls defined, so we can't use that).
  MaterialSetP allMatls = scinew MaterialSet();
  allMatls->createEmptySubsets(1);
  MaterialSubset* allMS = allMatls->getSubset(0);

  MaterialSetP prevMatlSet = 0;
  ConsecutiveRangeSet prevRangeSet;
  
  
  Task* t = scinew Task("UdaReducer::readAndSetVars", this, 
                        &UdaReducer::readAndSetVars);
  
  
  for (unsigned int i = 0; i < labels_.size(); i++) {
    VarLabel* label = labels_[i];

    ConsecutiveRangeSet matlsRangeSet;
    for (int i = 0; i < perProcPatches->getSubset(d_myworld->myrank())->size(); i++) {
      const Patch* patch = perProcPatches->getSubset(d_myworld->myrank())->get(i);
      matlsRangeSet = matlsRangeSet.unioned(dataArchive_->queryMaterials(label->getName(), patch, timeIndex_));
    }
    
    MaterialSet* matls;;
    if (prevMatlSet != 0 && prevRangeSet == matlsRangeSet) {
      matls = prevMatlSet.get_rep();
    }
    else {
      matls = scinew MaterialSet();
      vector<int> matls_vec;
      matls_vec.reserve(matlsRangeSet.size());
      
      for (ConsecutiveRangeSet::iterator iter = matlsRangeSet.begin(); iter != matlsRangeSet.end(); iter++) {
        if (!allMS->contains(*iter)) {
          allMS->add(*iter);
        }
	 matls_vec.push_back(*iter);
      }
      
      matls->addAll(matls_vec);
      prevRangeSet = matlsRangeSet;
      prevMatlSet  = matls;
    }

    allMS->sort();

    for (int l = 0; l < grid->numLevels(); l++) {
      t->computes(label, grid->getLevel(l)->allPatches()->getUnion(), matls->getUnion());
    }
  }

  //cout << d_myworld->myrank() << "  Done Calling DA::QuearyMaterials a bunch of times " << endl;
  MaterialSubsetP globalMatl = scinew MaterialSubset();
  t->setType(Task::OncePerProc);
  sched->addTask(t, perProcPatches, allMatls.get_rep());

  //__________________________________
  //  Carry forward delT

  Task* t2 = scinew Task("UdaReducer::computeDelT", this, 
                         &UdaReducer::computeDelT);
  globalMatl->add(-1);
  t2->computes(delt_label, grid->getLevel(0).get_rep(), globalMatl.get_rep());
  sched->addTask(t2, perProcPatches, allMatls.get_rep());
  
  
} // end scheduleTimeAdvance()
//______________________________________________________________________
//
void UdaReducer::initialize(const ProcessorGroup*,
			       const PatchSubset* patches,
			       const MaterialSubset* /*matls*/,
			       DataWarehouse* /*old_dw*/,
			       DataWarehouse* new_dw)
{

  double t = times_[0];
  delt_vartype delt_var = t; /* should subtract off start time --
				this assumes it's 0 */
  new_dw->put(delt_var, delt_label);  
}

//______________________________________________________________________
//
void UdaReducer::readAndSetVars(const ProcessorGroup*,
                                const PatchSubset* patches,
                                const MaterialSubset* matls,
                                DataWarehouse* /*old_dw*/,
                                DataWarehouse* new_dw)
{


  for(int p=0;p<patches->size();p++){ 
    const Patch* patch = patches->get(p);
    //__________________________________
    //output material indices
    if(patch->getID() == 0){

      cout << "//__________________________________Material Names:";
      int numAllMatls = d_sharedState->getNumMatls();
      cout << " numAllMatls " << numAllMatls << endl;
      for (int m = 0; m < numAllMatls; m++) {
        Material* matl = d_sharedState->getMaterial( m );
        cout <<" " << matl->getDWIndex() << ") " << matl->getName();
      }
      cout << "\n";
    }
  }


  double time = times_[timeIndex_];
  //int timestep = timesteps_[timeIndex_];

  if (timeIndex_ >= (int) (times_.size()-1)) {
    // error situation - we have run out of timesteps in the uda, but 
    // the time does not satisfy the maxTime, so the simulation wants to 
    // keep going
    cerr << "The timesteps in the uda directory do not extend to the maxTime\n"
         << "in the input.ups file.  To not get this exception, adjust the\n"
         << "maxTime in <udadir>/input.xml to be\n"
         << "between " << (times_.size() >= 3 ? times_[times_.size()-3] : 0)
         << " and " << times_[times_.size()-2] << " (the last time in the uda)\n"
         << "This is not a critical error - it just adds one more timestep\n"
         << "that you may have to remove manually\n\n";
  }
  
  proc0cout << "   Incrementing time " << timeIndex_ << " and time " << time << endl;


  dataArchive_->restartInitialize(timeIndex_, oldGrid_, new_dw, lb, &time);
  timeIndex_++;
}
//______________________________________________________________________
//
void UdaReducer::computeDelT(const ProcessorGroup*,
                             const PatchSubset* patches,
                             const MaterialSubset* matls,
                             DataWarehouse* /*old_dw*/,
                             DataWarehouse* new_dw)
  
{
  // don't use the delt produced in restartInitialize.
  double delt = times_[timeIndex_] - times_[timeIndex_-1];
  delt_vartype delt_var = delt;
  new_dw->put(delt_var, delt_label);
}

//______________________________________________________________________
//
double UdaReducer::getMaxTime()
{
  if (times_.size() <= 1){
    return 0;
  }else {
    return times_[times_.size()-2]; // the last one is the hacked one, see problemSetup
  }
}

//______________________________________________________________________
//
bool UdaReducer::needRecompile(double time, double dt,
                               const GridP& grid)
{
  bool recompile = gridChanged;
  gridChanged = false;

  vector<int> newNumMatls(oldGrid_->numLevels());
  
  for (int i = 0; i < oldGrid_->numLevels(); i++) {
    newNumMatls[i] = dataArchive_->queryNumMaterials(*oldGrid_->getLevel(i)->patchesBegin(), timeIndex_);
    
    if (i >=(int) numMaterials_.size() || numMaterials_[i] != newNumMatls[i]) {
      recompile = true;
    }
  }
  numMaterials_ = newNumMatls;
  return recompile;
}

//______________________________________________________________________
// called by the SimController once per timestep
GridP UdaReducer::getGrid() 
{ 
  GridP newGrid = dataArchive_->queryGrid(timeIndex_);
  
  if (oldGrid_ == 0 || !(*newGrid.get_rep() == *oldGrid_.get_rep())) {
    gridChanged = true;
    proc0cout << "     NEW GRID!!!!\n";
    oldGrid_ = newGrid;
    lb->possiblyDynamicallyReallocate(newGrid, true);
  }
  return oldGrid_; 
}
