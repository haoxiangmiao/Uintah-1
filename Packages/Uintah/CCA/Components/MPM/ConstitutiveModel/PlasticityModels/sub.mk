# Makefile fragment for this subdirectory

SRCDIR   := Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/PlasticityModels

SRCS     += \
	$(SRCDIR)/PlasticityState.cc \
	$(SRCDIR)/DamageModel.cc \
	$(SRCDIR)/DamageModelFactory.cc \
	$(SRCDIR)/NullDamage.cc \
	$(SRCDIR)/JohnsonCookDamage.cc \
	$(SRCDIR)/HancockMacKenzieDamage.cc \
	$(SRCDIR)/MPMEquationOfState.cc \
	$(SRCDIR)/MPMEquationOfStateFactory.cc \
	$(SRCDIR)/DefaultHypoElasticEOS.cc \
	$(SRCDIR)/HyperElasticEOS.cc \
	$(SRCDIR)/MieGruneisenEOS.cc \
	$(SRCDIR)/MieGruneisenEOSEnergy.cc \
	$(SRCDIR)/PlasticityModel.cc \
	$(SRCDIR)/PlasticityModelFactory.cc \
	$(SRCDIR)/IsoHardeningPlastic.cc \
	$(SRCDIR)/JohnsonCookPlastic.cc \
	$(SRCDIR)/ZAPlastic.cc\
	$(SRCDIR)/MTSPlastic.cc \
	$(SRCDIR)/SCGPlastic.cc \
	$(SRCDIR)/PTWPlastic.cc \
	$(SRCDIR)/YieldCondition.cc \
	$(SRCDIR)/YieldConditionFactory.cc \
	$(SRCDIR)/GursonYield.cc \
	$(SRCDIR)/VonMisesYield.cc \
	$(SRCDIR)/StabilityCheck.cc \
	$(SRCDIR)/StabilityCheckFactory.cc \
	$(SRCDIR)/BeckerCheck.cc \
	$(SRCDIR)/DruckerCheck.cc \
	$(SRCDIR)/NoneCheck.cc \
	$(SRCDIR)/DruckerBeckerCheck.cc \
	$(SRCDIR)/ShearModulusModel.cc \
	$(SRCDIR)/ShearModulusModelFactory.cc \
	$(SRCDIR)/ConstantShear.cc \
	$(SRCDIR)/MTSShear.cc \
	$(SRCDIR)/NPShear.cc \
	$(SRCDIR)/PTWShear.cc \
	$(SRCDIR)/SCGShear.cc \
	$(SRCDIR)/SpecificHeatModel.cc \
	$(SRCDIR)/SpecificHeatModelFactory.cc \
	$(SRCDIR)/ConstantCp.cc \
	$(SRCDIR)/CopperCp.cc \
	$(SRCDIR)/SteelCp.cc \
	$(SRCDIR)/MeltingTempModel.cc \
	$(SRCDIR)/MeltingTempModelFactory.cc \
	$(SRCDIR)/ConstantMeltTemp.cc \
	$(SRCDIR)/SCGMeltTemp.cc \
	$(SRCDIR)/BPSMeltTemp.cc \
	$(SRCDIR)/KinematicHardeningModel.cc \
	$(SRCDIR)/KinematicHardeningModelFactory.cc \
	$(SRCDIR)/NoKinematicHardening.cc \
	$(SRCDIR)/PragerKinematicHardening.cc \
	$(SRCDIR)/ArmstrongFrederickKinematicHardening.cc 

PSELIBS := Packages/Uintah/Core/Grid \
	Packages/Uintah/Core/Disclosure \
	Packages/Uintah/Core/Exceptions \
	Packages/Uintah/Core/Math \
	Packages/Uintah/Core/ProblemSpec \
	Packages/Uintah/CCA/Ports \
	Packages/Uintah/CCA/Components/ICE \
	Core/Datatypes \
	Core/Exceptions \
	Core/Geometry \
	Core/Math \
	Core/Util

LIBS    := 
