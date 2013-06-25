#!/usr/bin/env python
 
from sys import argv, exit
from os import environ
from helpers.runSusTests import runSusTests, inputs_root, generatingGoldStandards
from helpers.modUPS import modUPS

the_dir = generatingGoldStandards()

if the_dir == "" :
  the_dir = "%s/Wasatch" % inputs_root()
else :
  the_dir = the_dir + "/Wasatch"

liddrivencavityXYRe1000adaptive_ups = modUPS( the_dir,
                                       "lid-driven-cavity-xy-Re1000.ups",
                                       ["<delt_min>0.0001</delt_min>",
                                        "<delt_max>0.1</delt_max>",
                                        "<timestep_multiplier>0.5</timestep_multiplier>",
                                       "<filebase>liddrivencavityXYRe1000adaptive.uda</filebase>"])

liddrivencavity3DRe1000rk3_ups = modUPS( the_dir, \
                                       "lid-driven-cavity-3D-Re1000.ups", \
                                       ["<TimeIntegrator> RK3SSP </TimeIntegrator>", \
                                       "<filebase>liddrivencavity3DRe1000rk3.uda</filebase>"])
liddrivencavity3Dlaminarperf_ups = modUPS( the_dir, \
                                       "lid-driven-cavity-3D-Re1000.ups", \
                                       ["<max_Timesteps> 50 </max_Timesteps>","<resolution>[100,100,100]</resolution>","<patches>[1,1,1]</patches>"])
liddrivencavity3Dvremanperf_ups = modUPS( the_dir, \
                                       "turb-lid-driven-cavity-3D-VREMAN.ups", \
                                       ["<max_Timesteps> 50 </max_Timesteps>","<resolution>[100,100,100]</resolution>","<patches>[1,1,1]</patches>"])
liddrivencavity3Dsmagorinskyperf_ups = modUPS( the_dir, \
                                       "turb-lid-driven-cavity-3D-SMAGORINSKY.ups", \
                                       ["<max_Timesteps> 50 </max_Timesteps>","<resolution>[100,100,100]</resolution>","<patches>[1,1,1]</patches>"])
liddrivencavity3Dwaleperf_ups = modUPS( the_dir, \
                                       "turb-lid-driven-cavity-3D-WALE.ups", \
                                       ["<max_Timesteps> 50 </max_Timesteps>","<resolution>[100,100,100]</resolution>","<patches>[1,1,1]</patches>"])
scalabilitytestperf_ups = modUPS( the_dir, \
                                  "scalability-test.ups", \
                                  ["<max_Timesteps> 1000 </max_Timesteps>"])                                       

turbulenceDir = the_dir + "/TurbulenceVerification"

decayIsotropicTurbulenceCSmag32_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-csmag_32.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])
                                       
decayIsotropicTurbulenceCSmag64_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-csmag_64.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])
                                       
decayIsotropicTurbulenceVreman32_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-vreman_32.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])
                                       
decayIsotropicTurbulenceVreman64_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-vreman_64.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])
                                       
decayIsotropicTurbulenceWale32_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-wale_32.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])
                                       
decayIsotropicTurbulenceWale64_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-wale_64.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])

decayIsotropicTurbulenceDSmag32_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-dsmag_32.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])
                                       
decayIsotropicTurbulenceDSmag64_ups = modUPS( turbulenceDir, \
                                       "decay-isotropic-turbulence-dsmag_64.ups", \
                                       ["<max_Timesteps> 10 </max_Timesteps>","<outputTimestepInterval>1</outputTimestepInterval>",'<checkpoint cycle = "2" interval = "0.001"/>'])                                       

#______________________________________________________________________
#  Test syntax: ( "folder name", "input file", # processors, "OS", ["flags1","flag2"])
#  flags: 
#       gpu:                    - run test if machine is gpu enabled
#       no_uda_comparison:      - skip the uda comparisons
#       no_memoryTest:          - skip all memory checks
#       no_restart:             - skip the restart tests
#       no_dbg:                 - skip all debug compilation tests
#       no_opt:                 - skip all optimized compilation tests
#       do_performance_test:    - Run the performance test, log and plot simulation runtime.
#                                 (You cannot perform uda comparsions with this flag set)
#       doesTestRun:            - Checks if a test successfully runs
#       abs_tolerance=[double]  - absolute tolerance used in comparisons
#       rel_tolerance=[double]  - relative tolerance used in comparisons
#       exactComparison         - set absolute/relative tolerance = 0  for uda comparisons
#       startFromCheckpoint     - start test from checkpoint. (/home/csafe-tester/CheckPoints/..../testname.uda.000)
#       sus_options="string"    - Additional command line options for sus command
#
#  Notes: 
#  1) The "folder name" must be the same as input file without the extension.
#  2) If the processors is > 1.0 then an mpirun command will be used
#  3) Performance_tests are not run on a debug build.
#______________________________________________________________________

# to use these tests:
# export WHICH_TESTS=debug
# then make runLocalRT
# by default, local tests are executed. 
# To revert back to LocalTests, use:
# export WHICH_TESTS=local
DEBUGTESTS = [
  ("kinetic-energy-example",     "kinetic-energy-example.ups",   8,  "All",  ["exactComparison"] )  
]

UNUSED_TESTS = []

NIGHTLYTESTS = [
  ("reduction-test",       "reduction-test.ups",  4,  "Linux",  ["exactComparison","no_restart"] ),
  ("lid-drive-cavity-xy-Re1000-adaptive",       liddrivencavityXYRe1000adaptive_ups,  4,  "All",  ["exactComparison","no_restart"] ),
  ("decay-isotropic-turbulence-dsmag32",       "TurbulenceVerification/"+decayIsotropicTurbulenceDSmag32_ups,  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-dsmag64",       "TurbulenceVerification/"+decayIsotropicTurbulenceDSmag64_ups,  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-csmag32",       "TurbulenceVerification/"+decayIsotropicTurbulenceCSmag32_ups,  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-csmag64",       "TurbulenceVerification/"+decayIsotropicTurbulenceCSmag64_ups,  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-vreman32",      "TurbulenceVerification/"+decayIsotropicTurbulenceVreman32_ups, 8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-vreman64",      "TurbulenceVerification/"+decayIsotropicTurbulenceVreman64_ups, 8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-wale32",        "TurbulenceVerification/"+decayIsotropicTurbulenceWale32_ups,   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("decay-isotropic-turbulence-wale64",        "TurbulenceVerification/"+decayIsotropicTurbulenceWale64_ups,   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("turbulent-inlet-test-xminus",              "turbulent-inlet-test-xminus.ups",    12,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("turb-lid-driven-cavity-3D-VREMAN_perf",    liddrivencavity3Dvremanperf_ups,   1.1,  "Linux",  ["no_uda_comparison","no_restart","do_performance_test"] ),
  ("turb-lid-driven-cavity-3D-SMAGORINSKY_perf",   liddrivencavity3Dsmagorinskyperf_ups,   1.1,  "Linux",  ["no_uda_comparison","no_restart","do_performance_test"] ),
  ("turb-lid-driven-cavity-3D-WALE_perf",      liddrivencavity3Dwaleperf_ups,   1.1,  "Linux",  ["no_uda_comparison","no_restart","do_performance_test"] ),
  ("lid-driven-cavity-3D-LAMINAR_perf",        liddrivencavity3Dlaminarperf_ups,   1.1,  "Linux",  ["no_uda_comparison","no_restart","do_performance_test"] ),
  ("intrusion_flow_past_cylinder_xy",          "intrusion_flow_past_cylinder_xy.ups",    8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("intrusion_flow_past_cylinder_xz",          "intrusion_flow_past_cylinder_xz.ups",    8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("intrusion_flow_past_cylinder_yz",          "intrusion_flow_past_cylinder_yz.ups",    8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("intrusion_flow_past_objects_xy",           "intrusion_flow_past_objects_xy.ups",    16,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("intrusion_flow_over_icse",                 "intrusion_flow_over_icse.ups",          16,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("intrusion_flow_past_oscillating_cylinder_xy",          "intrusion_flow_past_oscillating_cylinder_xy.ups",    8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("turb-lid-driven-cavity-3D-VREMAN",   "turb-lid-driven-cavity-3D-VREMAN.ups",   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("projection_rk3_verification_dt0.01s",      "order-verification/projection_rk3_verification_dt0.01s.ups",   16,  "All",   ["exactComparison","no_restart","do_not_validate"] ),
  ("taylor-green-vortex-mms-pressure-src",      "taylor-green-vortex-mms-pressure-src.ups",   4,  "Linux",   ["exactComparison","no_restart","do_not_validate"] ),
  ("rk3-verification-ode",                     "rk3-verification-ode.ups",   1,  "Linux",   ["exactComparison","no_restart","do_not_validate"] ),
  ("rk3-verification-timedep-source",          "rk3-verification-timedep-source.ups",   1,  "Linux",   ["exactComparison","no_restart","do_not_validate"] ),
  ("bc-modifier-expression-test-multiple",   "bc-modifier-expression-test-multiple.ups",   8,  "Linux",   ["exactComparison","no_restart","do_not_validate"] ),
  ("read-from-file-test",   "read-from-file-test.ups",   8,  "Linux",   ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-symmetry-bc",   "channel-flow-symmetry-bc.ups",   6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("turb-lid-driven-cavity-3D-WALE",   "turb-lid-driven-cavity-3D-WALE.ups",   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("turb-lid-driven-cavity-3D-SMAGORINSKY",   "turb-lid-driven-cavity-3D-SMAGORINSKY.ups",   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("turb-lid-driven-cavity-3D-scalar",   "turb-lid-driven-cavity-3D-SMAGORINSKY-scalar.ups",   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("varden-projection-mms",                    "varden-projection-mms.ups",   3,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("varden-projection-advection-xdir",              "varden-projection-advection-xdir.ups",   3,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("varden-projection-advection-ydir",              "varden-projection-advection-ydir.ups",   3,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("varden-projection-advection-zdir",              "varden-projection-advection-zdir.ups",   3,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-xy-xminus-pressure-outlet",   "channel-flow-xy-xminus-pressure-outlet.ups",   6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-xy-xplus-pressure-outlet",    "channel-flow-xy-xplus-pressure-outlet.ups",    6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-xz-zminus-pressure-outlet",   "channel-flow-xz-zminus-pressure-outlet.ups",   6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-xz-zplus-pressure-outlet",    "channel-flow-xz-zplus-pressure-outlet.ups",    6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-yz-yminus-pressure-outlet",   "channel-flow-yz-yminus-pressure-outlet.ups",   6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("channel-flow-yz-yplus-pressure-outlet",    "channel-flow-yz-yplus-pressure-outlet.ups",    6,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("lid-driven-cavity-3D-Re1000",   "lid-driven-cavity-3D-Re1000.ups",   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("liddrivencavity3DRe1000rk3",    liddrivencavity3DRe1000rk3_ups,   8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("lid-driven-cavity-xy-Re1000",   "lid-driven-cavity-xy-Re1000.ups",   4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("lid-driven-cavity-xz-Re1000",   "lid-driven-cavity-xz-Re1000.ups",   4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("lid-driven-cavity-yz-Re1000",   "lid-driven-cavity-yz-Re1000.ups",   4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("hydrostatic-pressure-test",     "hydrostatic-pressure-test.ups",     8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("BasicScalarTransportEquation",  "BasicScalarTransportEquation.ups",  1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("BasicScalarTransportEq_2L",     "BasicScalarTransportEq_2L.ups",     1,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("TabPropsInterface",             "TabPropsInterface.ups",             1,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("bc-test-mixed",                 "bc-test-mixed.ups",                 4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("ScalarTransportEquation",       "ScalarTransportEquation.ups",       1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("taylor-green-vortex-2d-xy",          "taylor-green-vortex-2d-xy.ups",          4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("taylor-green-vortex-2d-xz",          "taylor-green-vortex-2d-xz.ups",          4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("taylor-green-vortex-2d-yz",          "taylor-green-vortex-2d-yz.ups",          4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("scalability-test",              "scalability-test.ups",              1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("taylor-green-vortex-3d",          "taylor-green-vortex-3d.ups",          8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("bc-test-svol-xdir",             "bc-test-svol-xdir.ups",             4,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("bc-test-svol-ydir",             "bc-test-svol-ydir.ups",             4,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("bc-test-svol-zdir",             "bc-test-svol-zdir.ups",             4,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("qmom-realizable-test",          "qmom-realizable-test.ups",          8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("qmom-test",                     "qmom-test.ups",                     4,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("qmom-aggregation-test",         "qmom-aggregation-test.ups",         1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("qmom-birth-test",               "qmom-birth-test.ups",               1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("qmom-ostwald-test",             "qmom-ostwald-test.ups",             1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("qmom-surface-energy-test",      "qmom-surface-energy-test.ups",      1,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-xdir",     "convection-test-svol-xdir.ups",     4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-ydir",     "convection-test-svol-ydir.ups",     4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-zdir",     "convection-test-svol-zdir.ups",     4,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-xdir-bc",  "convection-test-svol-xdir-bc.ups",  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-ydir-bc",  "convection-test-svol-ydir-bc.ups",  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-zdir-bc",  "convection-test-svol-zdir-bc.ups",  8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("convection-test-svol-mixed-bc", "convection-test-svol-mixed-bc.ups", 8,  "Linux",  ["exactComparison","no_restart","do_not_validate"] ),
  ("force-on-graph-postprocessing-test",     "force-on-graph-postprocessing-test.ups",   4,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("kinetic-energy-example",     "kinetic-energy-example.ups",   8,  "Linux",  ["exactComparison","no_restart"] )  
]

#  ("radprops",                      "RadPropsInterface.ups",             2,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] )

# Tests that are run during local regression testing
LOCALTESTS = [
  ("reduction-test",       "reduction-test.ups",  4,  "All",  ["exactComparison","no_restart"] ),
  ("lid-drive-cavity-xy-Re1000-adaptive",       liddrivencavityXYRe1000adaptive_ups,  4,  "All",  ["exactComparison","no_restart"] ),
  ("decay-isotropic-turbulence-dsmag32",       "TurbulenceVerification/"+decayIsotropicTurbulenceDSmag32_ups,  8,  "All",  ["exactComparison"] ), 
  ("decay-isotropic-turbulence-dsmag64",       "TurbulenceVerification/"+decayIsotropicTurbulenceDSmag64_ups,  8,  "All",  ["exactComparison","no_restart"] ), 
  ("decay-isotropic-turbulence-csmag32",       "TurbulenceVerification/"+decayIsotropicTurbulenceCSmag32_ups,  8,  "All",  ["exactComparison"] ), 
  ("decay-isotropic-turbulence-csmag64",       "TurbulenceVerification/"+decayIsotropicTurbulenceCSmag64_ups,  8,  "All",  ["exactComparison","no_restart"] ), 
  ("decay-isotropic-turbulence-vreman32",      "TurbulenceVerification/"+decayIsotropicTurbulenceVreman32_ups, 8,  "All",  ["exactComparison"] ), 
  ("decay-isotropic-turbulence-vreman64",      "TurbulenceVerification/"+decayIsotropicTurbulenceVreman64_ups, 8,  "All",  ["exactComparison","no_restart"] ), 
  ("decay-isotropic-turbulence-wale32",        "TurbulenceVerification/"+decayIsotropicTurbulenceWale32_ups,   8,  "All",  ["exactComparison"] ), 
  ("decay-isotropic-turbulence-wale64",        "TurbulenceVerification/"+decayIsotropicTurbulenceWale64_ups,   8,  "All",  ["exactComparison","no_restart"] ), 
  ("turbulent-inlet-test-xminus",              "turbulent-inlet-test-xminus.ups",    12,  "All",  ["exactComparison","no_restart"] ),                         
  ("intrusion_flow_past_cylinder_xy",          "intrusion_flow_past_cylinder_xy.ups",    8,  "All",  ["exactComparison","no_restart"] ),                         
  ("intrusion_flow_past_cylinder_xz",          "intrusion_flow_past_cylinder_xz.ups",    8,  "All",  ["exactComparison","no_restart"] ),                         
  ("intrusion_flow_past_cylinder_yz",          "intrusion_flow_past_cylinder_yz.ups",    8,  "All",  ["exactComparison","no_restart"] ),                         
  ("intrusion_flow_past_objects_xy",           "intrusion_flow_past_objects_xy.ups",    16,  "All",  ["exactComparison","no_restart"] ),                         
  ("intrusion_flow_over_icse",                 "intrusion_flow_over_icse.ups",          16,  "All",  ["exactComparison","no_restart"] ),                         
  ("intrusion_flow_past_oscillating_cylinder_xy",          "intrusion_flow_past_oscillating_cylinder_xy.ups",    8,  "All",  ["exactComparison","no_restart"] ), 
  ("turb-lid-driven-cavity-3D-VREMAN",         "turb-lid-driven-cavity-3D-VREMAN.ups",   8,  "All",  ["exactComparison","no_restart"] ),                         
  ("projection_rk3_verification_dt0.01s",      "order-verification/projection_rk3_verification_dt0.01s.ups",   16,  "All",   ["exactComparison","no_restart"] ), 
  ("taylor-green-vortex-mms-pressure-src",      "taylor-green-vortex-mms-pressure-src.ups",   4,  "All",   ["exactComparison","no_restart"] ),                 
  ("rk3-verification-ode",                     "rk3-verification-ode.ups",   1,  "All",   ["exactComparison","no_restart"] ),                                
  ("rk3-verification-timedep-source",          "rk3-verification-timedep-source.ups",   1,  "All",   ["exactComparison","no_restart"] ),                     
  ("bc-modifier-expression-test-multiple",     "bc-modifier-expression-test-multiple.ups",   8,  "All",   ["exactComparison","no_restart"] ),                
  ("read-from-file-test",                      "read-from-file-test.ups",   8,  "All",   ["exactComparison","no_restart"] ),                                 
  ("channel-flow-symmetry-bc",                 "channel-flow-symmetry-bc.ups",   6,  "All",   ["exactComparison","no_restart"] ),                            
  ("turb-lid-driven-cavity-3D-WALE",           "turb-lid-driven-cavity-3D-WALE.ups",   8,  "All",  ["exactComparison","no_restart"] ),                       
  ("turb-lid-driven-cavity-3D-SMAGORINSKY",    "turb-lid-driven-cavity-3D-SMAGORINSKY.ups",   8,  "All",  ["exactComparison","no_restart"] ),                
  ("turb-lid-driven-cavity-3D-scalar",         "turb-lid-driven-cavity-3D-SMAGORINSKY-scalar.ups",   8,  "All",  ["exactComparison","no_restart"] ),         
  ("varden-projection-mms",                    "varden-projection-mms.ups",   3,  "All",  ["exactComparison","no_restart"] ),         
  ("varden-projection-advection-xdir",              "varden-projection-advection-xdir.ups",   3,  "All",  ["exactComparison","no_restart"] ),         
  ("varden-projection-advection-ydir",              "varden-projection-advection-ydir.ups",   3,  "All",  ["exactComparison","no_restart"] ),         
  ("varden-projection-advection-zdir",              "varden-projection-advection-zdir.ups",   3,  "All",  ["exactComparison","no_restart"] ),         
  ("channel-flow-xy-xminus-pressure-outlet",   "channel-flow-xy-xminus-pressure-outlet.ups",   6,  "All",  ["exactComparison","no_restart"] ),               
  ("channel-flow-xy-xplus-pressure-outlet",    "channel-flow-xy-xplus-pressure-outlet.ups",    6,  "All",  ["exactComparison","no_restart"] ),               
  ("channel-flow-xz-zminus-pressure-outlet",   "channel-flow-xz-zminus-pressure-outlet.ups",   6,  "All",  ["exactComparison","no_restart"] ),               
  ("channel-flow-xz-zplus-pressure-outlet",    "channel-flow-xz-zplus-pressure-outlet.ups",    6,  "All",  ["exactComparison","no_restart"] ),               
  ("channel-flow-yz-yminus-pressure-outlet",   "channel-flow-yz-yminus-pressure-outlet.ups",   6,  "All",  ["exactComparison","no_restart"] ),               
  ("channel-flow-yz-yplus-pressure-outlet",    "channel-flow-yz-yplus-pressure-outlet.ups",    6,  "All",  ["exactComparison","no_restart"] ),               
  ("lid-driven-cavity-3D-Re1000",   "lid-driven-cavity-3D-Re1000.ups",   8,  "All",   ["exactComparison"] ),                
  ("liddrivencavity3DRe1000rk3",   liddrivencavity3DRe1000rk3_ups,   8,  "All",  ["exactComparison","no_restart"] ),                        
  ("lid-driven-cavity-xy-Re1000",   "lid-driven-cavity-xy-Re1000.ups",   4,  "All",   ["exactComparison","no_restart"] ),                   
  ("lid-driven-cavity-xz-Re1000",   "lid-driven-cavity-xz-Re1000.ups",   4,  "All",   ["exactComparison","no_restart"] ),                   
  ("lid-driven-cavity-yz-Re1000",   "lid-driven-cavity-yz-Re1000.ups",   4,  "All",   ["exactComparison","no_restart"] ),                   
  ("hydrostatic-pressure-test",     "hydrostatic-pressure-test.ups",     8,  "All",   ["exactComparison","no_restart"] ),                   
  ("BasicScalarTransportEquation", "BasicScalarTransportEquation.ups",   1,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("BasicScalarTransportEq_2L",     "BasicScalarTransportEq_2L.ups",     1,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("TabPropsInterface",             "TabPropsInterface.ups",             1,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("bc-test-mixed",                 "bc-test-mixed.ups",                 4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("ScalarTransportEquation",       "ScalarTransportEquation.ups",       1,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("taylor-green-vortex-2d-xy",          "taylor-green-vortex-2d-xy.ups",          4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("taylor-green-vortex-2d-xz",          "taylor-green-vortex-2d-xz.ups",          4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("taylor-green-vortex-2d-yz",          "taylor-green-vortex-2d-yz.ups",          4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("scalability-test",              "scalability-test.ups",              1,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("taylor-green-vortex-3d",          "taylor-green-vortex-3d.ups",          8,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("bc-test-svol-xdir",             "bc-test-svol-xdir.ups",             4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("bc-test-svol-ydir",             "bc-test-svol-ydir.ups",             4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("bc-test-svol-zdir",             "bc-test-svol-zdir.ups",             4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("qmom-realizable-test",          "qmom-realizable-test.ups",          8,  "All",   ["exactComparison","no_restart"] ),   
  ("qmom-test",                     "qmom-test.ups",                     4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("qmom-aggregation-test",         "qmom-aggregation-test.ups",         1,  "All",  ["exactComparison","no_restart"] ),   
  ("qmom-birth-test",               "qmom-birth-test.ups",               1,  "All",  ["exactComparison","no_restart"] ),   
  ("qmom-ostwald-test",             "qmom-ostwald-test.ups",             1,  "All",  ["exactComparison","no_restart"] ),
  ("qmom-surface-energy-test",      "qmom-surface-energy-test.ups",      1,  "All",  ["exactComparison","no_restart"] ),    
  ("convection-test-svol-xdir",     "convection-test-svol-xdir.ups",     4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("convection-test-svol-ydir",     "convection-test-svol-ydir.ups",     4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("convection-test-svol-zdir",     "convection-test-svol-zdir.ups",     4,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("convection-test-svol-xdir-bc",  "convection-test-svol-xdir-bc.ups",  8,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("convection-test-svol-ydir-bc",  "convection-test-svol-ydir-bc.ups",  8,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("convection-test-svol-zdir-bc",  "convection-test-svol-zdir-bc.ups",  8,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("convection-test-svol-mixed-bc", "convection-test-svol-mixed-bc.ups", 8,  "All",   ["exactComparison","no_restart","no_memoryTest"] ),   
  ("force-on-graph-postprocessing-test",     "force-on-graph-postprocessing-test.ups",   4,  "All",  ["exactComparison","no_restart","no_memoryTest"] ),
  ("kinetic-energy-example",     "kinetic-energy-example.ups",   8,  "All",  ["exactComparison","no_restart"] )  
]

#  ("radprops",                      "RadPropsInterface.ups",             2,  "Linux",  ["exactComparison","no_restart","no_memoryTest"] )
  
#__________________________________

def getNightlyTests() :
  return NIGHTLYTESTS

def getLocalTests() :
  return LOCALTESTS

#__________________________________

if __name__ == "__main__":

  if environ['WHICH_TESTS'] == "local":
    TESTS = LOCALTESTS
  elif environ['WHICH_TESTS'] == "debug":
    TESTS = DEBUGTESTS
  else:
    TESTS = NIGHTLYTESTS

  result = runSusTests(argv, TESTS, "Wasatch")
  exit( result )

