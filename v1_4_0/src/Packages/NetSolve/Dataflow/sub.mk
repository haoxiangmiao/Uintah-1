#Makefile fragment for the Packages/Netsolve/Dataflow directory

include $(SCIRUN_SCRIPTS)/largeso_prologue.mk

SRCDIR := Packages/NetSolve/Dataflow
SUBDIRS := \
	$(SRCDIR)/GUI \
	$(SRCDIR)/Modules \

include $(SCIRUN_SCRIPTS)/recurse.mk

PSELIBS := 
LIBS := $(TK_LIBRARY) $(GL_LIBS) -lm

include $(SCIRUN_SCRIPTS)/largeso_epilogue.mk
