/*
 * The MIT License
 *
 * Copyright (c) 1997-2018 The University of Utah
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

#ifndef UINTAH_CCA_COMPONENTS_MPMFVM_ESMPM_H
#define UINTAH_CCA_COMPONENTS_MPMFVM_ESMPM_H

#include <CCA/Components/Application/ApplicationCommon.h>

#include <CCA/Components/FVM/FVMLabel.h>
#include <CCA/Components/FVM/ElectrostaticSolve.h>
#include <CCA/Components/MPMFVM/ESConductivityModel.h>
#include <CCA/Components/MPM/AMRMPM.h>
#include <CCA/Components/MPM/Core/MPMFlags.h>
#include <CCA/Ports/DataWarehouse.h>
#include <CCA/Ports/Output.h>
#include <CCA/Ports/Scheduler.h>
#include <CCA/Ports/SwitchingCriteria.h>
#include <Core/Geometry/IntVector.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/Vector.h>
#include <Core/Grid/Ghost.h>
#include <Core/Grid/Grid.h>
#include <Core/Grid/Level.h>
#include <Core/Grid/Variables/ComputeSet.h>
#include <Core/Labels/MPMLabel.h>
#include <Core/Parallel/ProcessorGroup.h>
#include <Core/ProblemSpec/ProblemSpec.h>

#include <vector>
#include <string>

namespace Uintah {
  class ESMPM : public ApplicationCommon {
    public:
      ESMPM(const ProcessorGroup* myworld,
	    const SimulationStateP sharedState);
    
      ~ESMPM();

      virtual void problemSetup(const ProblemSpecP& prob_spec,
                                const ProblemSpecP& restart_prob_spec,
                                GridP& grid);

      virtual void outputProblemSpec(ProblemSpecP& prob_spec);

      virtual void scheduleInitialize(const LevelP& level, SchedulerP& sched);

      virtual void scheduleRestartInitialize(const LevelP& level, SchedulerP& sched);

      virtual void restartInitialize();

      virtual void scheduleComputeStableTimeStep(const LevelP& level, SchedulerP& sched);

      virtual void scheduleTimeAdvance( const LevelP& level, SchedulerP& sched);

      virtual void scheduleFinalizeTimestep(const LevelP& level, SchedulerP& sched);

      virtual void scheduleInterpESPotentialToPart(SchedulerP& sched,
                                                   const PatchSet* patches,
                                                   const MaterialSubset* mpm_matls,
                                                   const MaterialSubset* es_matls,
                                                   const MaterialSet* all_matls);

    protected:
      virtual void initialize(const ProcessorGroup*, const PatchSubset* patches,
                              const MaterialSubset* matls,
                              DataWarehouse*,
                              DataWarehouse* new_dw);

      virtual void interpESPotentialToPart(const ProcessorGroup*,
                                           const PatchSubset* patches,
                                           const MaterialSubset* ,
                                           DataWarehouse* old_dw,
                                           DataWarehouse* new_dw);

    private:
      double d_TINY_RHO;
      std::string d_cd_model_name;

      AMRMPM* d_amrmpm;
      ElectrostaticSolve* d_esfvm;
      MPMLabel* d_mpm_lb;
      FVMLabel* d_fvm_lb;
      MPMFlags* d_mpm_flags;
      ESConductivityModel* d_conductivity_model;

      MaterialSet* d_es_matlset;
      MaterialSubset* d_es_matl;
      SwitchingCriteria* d_switch_criteria;

      Ghost::GhostType d_gac;
  };
}
#endif
