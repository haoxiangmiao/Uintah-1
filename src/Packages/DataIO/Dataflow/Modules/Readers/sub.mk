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

INCLUDES += $(TEEM_INCLUDE) $(HDF5_INCLUDE) $(MDSPLUS_INCLUDE)

SRCDIR   := Packages/DataIO/Dataflow/Modules/Readers

SRCS     += \
	$(SRCDIR)/MDSPlusDataReader.cc\
	$(SRCDIR)/MDSPlusFieldReader.cc\
	$(SRCDIR)/HDF5DataReader.cc\
	$(SRCDIR)/HDF5Dump.cc\
#[INSERT NEW CODE FILE HERE]

PSELIBS := Core/Datatypes Dataflow/Network Dataflow/Ports \
        Core/Persistent Core/Containers Core/Util \
        Core/Exceptions Core/Thread Core/GuiInterface \
        Core/Geom Core/Datatypes Core/Geometry \
        Core/TkExtensions \
	Packages/DataIO/Core/ThirdParty \
	Packages/Teem/Core/Datatypes

LIBS := $(TK_LIBRARY) $(GL_LIBRARY) $(M_LIBRARY) $(TEEM_LIBRARY) $(MDSPLUS_LIBRARY) $(HDF5_LIBRARY)

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk


