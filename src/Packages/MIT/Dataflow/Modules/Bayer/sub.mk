# *** NOTE ***
#
# Do not remove or modify the comment line:
#
# #[INSERT NEW ?????? HERE]
#
# It is required by the Component Wizard to properly edit this file.
# if you want to edit this file by hand, see the "Create A New Component"
# documentation on how to do it correctly.

include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR   := Packages/MIT/Dataflow/Modules/Bayer

SRCS     += \
	$(SRCDIR)/Metropolis.cc\
        $(SRCDIR)/vmnorm.F\
        $(SRCDIR)/zufall.F\
	$(SRCDIR)/ThetaView.cc\
        $(SRCDIR)/bayer.F\
        $(SRCDIR)/lsoda.F\
        $(SRCDIR)/bnorm.F\
        $(SRCDIR)/cfode.F\
        $(SRCDIR)/d1mach.F\
        $(SRCDIR)/daxpy.F\
        $(SRCDIR)/ddot.F\
        $(SRCDIR)/dgbfa.F\
        $(SRCDIR)/dgbsl.F\
        $(SRCDIR)/dgefa.F\
        $(SRCDIR)/dgesl.F\
        $(SRCDIR)/dscal.F\
        $(SRCDIR)/ewset.F\
        $(SRCDIR)/fnorm.F\
        $(SRCDIR)/idamax.F\
        $(SRCDIR)/intdy.F\
        $(SRCDIR)/prja.F\
        $(SRCDIR)/solsy.F\
        $(SRCDIR)/stoda.F\
        $(SRCDIR)/xerrwv.F\
#[INSERT NEW CODE FILE HERE]

#	$(SRCDIR)/Bayer.cc \

PSELIBS := Packages/MIT/Core/Datatypes \
	Dataflow/Network Dataflow/Ports \
	Core/2d \
	Core/Datatypes \
        Core/Persistent Core/Containers Core/Util \
        Core/Exceptions Core/Thread Core/GuiInterface \
        Core/Geom Core/Datatypes Core/Geometry \
        Core/TkExtensions
LIBS := $(GL_LIBRARY) $(TK_LIBRARY) \
	-lcvode\
	-L/usr/local/lib -lunuran  \
	-llapack  -lblas -lg2c -lm

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk


