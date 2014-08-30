#
#  The MIT License
#
#  Copyright (c) 1997-2014 The University of Utah
# 
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to
#  deal in the Software without restriction, including without limitation the
#  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#  IN THE SOFTWARE.



# configVars.mk.in holds all of the variables needed by the make 
# system to create makefiles. Various Makefiles can include this central
# copy of the vars. This file has all of the variables and rules common to 
# all generated makefiles.

TAU_MAKEFILE := 
ifneq ($(TAU_MAKEFILE),)
  include $(TAU_MAKEFILE)
  PDTPARSE = $(PDTDIR)/$(PDTARCHDIR)/bin/cxxparse
  TAUINSTR = $(TAUROOT)/$(CONFIG_ARCH)/bin/tau_instrumentor
endif

# 'a' if archives, 'so' if shared libs.
SO_OR_A_FILE := so
IS_STATIC_BUILD := no

# Blow away a bunch of makes internal rules to improve the performance
# of make
.SUFFIXES:
% :: RCS/%,v
% :: RCS/%
% :: %,v
% :: s.%
% :: SCCS/s.%
%.out :: %
%.c :: %
%.tex :: %


ifeq ($(OBJTOP),.)
  LIBDIR := lib
else
  LIBDIR := $(OBJTOP_ABS)/lib
endif

# squash any output from noisy environments, we just want the pwd output
LIBDIR_ABS    := $(shell mkdir -p $(LIBDIR) > /dev/null; cd $(LIBDIR)\
	           > /dev/null; pwd) 

SCIRUN_LIBDIR := $(LIBDIR)

# squash any output from noisy environments, we just want the pwd output
SCIRUN_LIBDIR_ABS := $(shell cd $(SCIRUN_LIBDIR) > /dev/null; pwd)

VPATH := $(SRCTOP)

# Optional pieces
BUILD_VISIT := 
VISIT_INSTALL_DIR := 

# These cannot be :=
THREAD_IMPL = $(SRCDIR)/Thread_pthreads.cc
TIME_IMPL = $(SRCDIR)/Time_unix.cc
REFCOUNT_IMPL = $(SRCDIR)/RefCounted_gcc.cc
ATOMIC_IMPL = $(SRCDIR)/AtomicCounter_gcc.cc

# Subdirectories
SUBDIRS := Core CCA tools StandAlone VisIt

SUBDIRS += $(COMPONENT_DIRS)

SUBDIRS += testprograms

IS_OSX := no
IS_AIX := no
IS_REDSTORM := no
IS_BGQ := 
MAKE_ARCHIVES := no
IS_DEBUG := yes

# Set to 'yes' if Fortran is disabled:
NO_FORTRAN := 

SCI_MALLOC_ON := no

NEED_OSX_SYSTEMSTUBS := 
NEED_OSX_HACK := @NEED_OSX_HACK@
ifeq ($(NEED_OSX_HACK),yes)
  SUBDIRS := osx $(SUBDIRS)
endif

LDRUN_PREFIX      := -Wl,-rpath -Wl,

ifeq ($(SCIRUN_APP_NAME),)
  SCIRUN_APP_NAME := SCIRun
endif

# Libraries and other flags

M_LIBRARY :=  -lm

HAVE_TEEM    := 
TEEM_LIBRARY :=  
TEEM_INCLUDE := 

#ViSUS (nvisusio)
HAVE_VISUS := 
VISUS_LIBRARY :=  
VISUS_INCLUDE := 

#ViSUS (Parallel IDX)
HAVE_PIDX := 
PIDX_LIBRARY :=  
PIDX_INCLUDE := 

FREETYPE_LIBRARY :=  
FREETYPE_INCLUDE := 

FTGL_LIBRARY :=   @FTGL_LIB_DIR_FLAG@ @FTGL_LIB_FLAG@
FTGL_INCLUDE :=  @INC_FTGL_H@

SCISOCK_LIBRARY :=  
SCISOCK_INCLUDE := 

# MallocTrace
MALLOC_TRACE_INCLUDE = 
MALLOC_TRACE_LIBRARY =  

# Zoltan
ZOLTAN_INCLUDE = 
ZOLTAN_LIBRARY =  

# Boost
HAVE_BOOST    := yes
BOOST_INCLUDE := -I/usr/local/boost/include
BOOST_LIBRARY := -Wl,-rpath -Wl,/usr/local/boost/lib -L/usr/local/boost/lib -lboost_thread -lboost_signals -lboost_serialization -lboost_system -lboost_program_options -lboost_filesystem

# SpatialOps
HAVE_SPATIALOPS    := yes
SPATIALOPS_INCLUDE := -I/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/SpatialOps/include
SPATIALOPS_LIBRARY := -L/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/SpatialOps/lib/spatialops -lspatialops -lspatialops-stencil -lspatialops-structured

# ExprLib
HAVE_EXPRLIB    := yes
EXPRLIB_INCLUDE := -I/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/ExprLib/include
EXPRLIB_LIBRARY := -L/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/ExprLib/lib/expression -lexprlib

# TabProps
TABPROPS_INCLUDE =  -I/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/TabProps/include
TABPROPS_LIBRARY =  -L/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/TabProps/lib/tabprops -ltabprops -linterp $(BOOST_LIBRARY)
HAVE_TABPROPS    := yes

# RadProps
RADPROPS_INCLUDE =  -I/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/RadProps/include
RADPROPS_LIBRARY =  -L/home/alan/uintah-md/md-dev/dbg-test-all/Wasatch3P/install/RadProps/lib/radprops -lradprops $(BOOST_LIBRARY)
HAVE_RADPROPS    := yes

HAVE_BLAS      := yes
HAVE_CBLAS     := yes
BLAS_LIBRARY   :=  -lblas
BLAS_INCLUDE   := 

# Note: If we are not on an SGI, then LAPACKMP will be the same as
# LAPACK!
HAVE_LAPACK    := yes
HAVE_LAPACKMP  := yes
LAPACK_LIBRARY :=  -llapack
LAPACKMP_LIBRARY :=  -llapack

ifneq ($(TAU_MAKEFILE),)
  TAU_LIB_DIR       := $(TAU_PREFIX_INSTALL_DIR)/$(CONFIG_ARCH)/lib
  TAU_LIB_FLAG      := $(TAU_MPI_LIBS) $(TAU_SHLIBS)

  CFLAGS := $(CFLAGS) -DUSE_TAU_PROFILING $(TAU_DEFS)
  CXXFLAGS := $(CXXFLAGS) -DUSE_TAU_PROFILING $(TAU_DEFS)
  TAU_LIBRARY := $(LDRUN_PREFIX)$(TAU_LIB_DIR) -L$(TAU_LIB_DIR) $(TAU_LIB_FLAG)
else
  TAU_LIBRARY :=
endif

ifeq ($(IS_BGQ),yes)
  CFLAGS   := $(CFLAGS) -DPETSC_RESTRICT=__restrict__ -DPETSC_DEPRECATED\(why\)= 
  CXXFLAGS := $(CXXFLAGS) -DPETSC_RESTRICT=__restrict__ -DPETSC_DEPRECATED\(why\)= 
endif

PAPI_INCLUDE     := 
PAPI_LIBRARY     :=  

GPERFTOOLS_INCLUDE     := 
GPERFTOOLS_LIBRARY     :=  

FFTW_LIBRARY     := -Wl,-rpath -Wl,/usr/local/fftw/lib -L/usr/local/fftw/lib -lfftw3 -lfftw3_mpi -lfftw3_threads
FFTW_INCLUDE     := -I/usr/local/fftw/include

# Always put $(TAU_LIBRARY) in front of $(MPI_LIBRARY) for the TAU_MPI wrapper

MPI_LIBRARY := $(TAU_LIBRARY) -Wl,-rpath -Wl,/usr/local/openmpi/lib -L/usr/local/openmpi/lib  -lmpi -lrt -lgfortran 
MPI_INCLUDE := -I/usr/local/openmpi/include

# not blank.  Its "value" is not useful.
HAVE_MPI := yes

HAVE_MAGMA        := 
MAGMA_INCLUDE     := 
MAGMA_LIBRARY     :=  

HAVE_CUDA        := 
CUDA_INCLUDE     := 
CUDA_LIBRARY     :=  

MDSPLUS_LIBRARY := @MDSPLUS_LIB_DIR_FLAG@ @MDSPLUS_LIB_FLAG@
MDSPLUS_INCLUDE := @INC_MDSPLUS_H@

HDF5_LIBRARY :=  
HDF5_INCLUDE := 

XALAN_PATH := 

UNP_LIBRARY :=  
UNP_INCLUDE := 

THREAD_LIBRARY := -lpthread
SEMAPHORE_LIB := 
SOCKET_LIBRARY := 

PROGRAM_PSE   := scirun

TRACEBACK_LIB :=  

PERL := /usr/bin/perl
SED := sed

MAKE_PARALLELISM := 8

LARGESOS := no

ifeq ($(LARGESOS),yes)
  MALLOCLIB := Core
else
  MALLOCLIB := Core/Malloc
endif

# Convenience variables - compounded from above definitions

DL_LIBRARY :=  -ldl
Z_LIBRARY :=  -lz 

XML2_LIBRARY := $(Z_LIBRARY)  -lxml2
XML2_INCLUDE := -I/usr/include/libxml2

HAVE_TIFF := no
JPEG_LIBRARY :=  
TIFF_LIBRARY :=  

HAVE_KEPLER := @HAVE_KEPLER@

VDT_INCLUDE := @INC_VDT_H@
VDT_LIBRARY := @VDT_LIB_DIR_FLAG@ @VDT_LIB_FLAG@

HAVE_HYPRE     := yes
HYPRE_INCLUDE  := -I/usr/local/hypre/include
HYPRE_LIBRARY  := -Wl,-rpath -Wl,/usr/local/hypre/lib -L/usr/local/hypre/lib -lHYPRE

HAVE_PETSC     := yes
PETSC_INCLUDE  := -I/usr/local/petsc/include
PETSC_LIBRARY  := -Wl,-rpath -Wl,/usr/local/petsc/lib -L/usr/local/petsc/lib -lpetsc


ELECTRIC_FENCE :=  

INCLUDES += -I$(SRCTOP)/include -I$(SRCTOP) -I$(OBJTOP) -I$(OBJTOP)/include $(XML2_INCLUDE) $(MPI_INCLUDE) $(PAPI_INCLUDE) $(FFTW_INCLUDE) $(GPERFTOOLS_INCLUDE) $(CUDA_INCLUDE) $(MAGMA_INCLUDE)  $(TAUINC) $(ZOLTAN_INCLUDE) $(TABPROPS_INCLUDE) ${MALLOC_TRACE_INCLUDE}
FINCLUDES += -I$(SRCTOP) -I$(OBJTOP) -I$(OBJTOP)/include

BUILD_ARCHES=yes
BUILD_ICE=yes
BUILD_FVM=yes
BUILD_MODELS_RADIATION=yes
BUILD_MPM=yes
BUILD_MD=yes
BUILD_WASATCH=yes
BUILD_WASATCH_IN_ARCHES=no

SSTREAM_COMPAT := no
ifeq ($(SSTREAM_COMPAT),yes)
  INCLUDES := $(INCLUDES) -I$(SRCTOP)/include/compat
endif

################################################################
# Variables and suffix rules for module transformation:
#

CC              := gcc
CXX             := g++
NVCC            := 
AS              := as
LD              := ld
CFLAGS          := -fPIC  -fopenmp -Wall  -g -O0 -fno-inline-functions  $(CFLAGS) 
CXXFLAGS        := -fPIC  -fopenmp -Wall  -g -O0 -fno-inline-functions  $(CXXFLAGS) 
NVCC_CFLAGS     := 
NVCC_CXXFLAGS   := 
SOFLAGS         := $(CFLAGS) -shared  -L$(LIBDIR)
LDFLAGS         := $(CXXFLAGS)  -L$(LIBDIR) $(ELECTRIC_FENCE)
NVCC_LDFLAGS    := $(NVCC_CXXFLAGS) 
ASFLAGS         :=   $(ASFLAGS)
F77             := gfortran
FFLAGS          := -fPIC  -fbounds-check  -g -O0 -fno-inline-functions  $(FFLAGS)

# Fortran Library:
F_LIBRARY	:=  -lgfortran -lrt
REPOSITORY_FLAGS := 
OBJEXT := o

################################################################
# When building a static executable, these are the basic PSE libraries that are needed.  (Order matters!)
# Note, for some of the Uintah executables (most importantly, for 'sus') many more libraries are needed
# and these libraries are listed in the individual sub.mk file for that executable.
#
CORE_STATIC_PSELIBS := \
    Core_DataArchive Core_Grid Core_ProblemSpec Core_GeometryPiece CCA_Components_ProblemSpecification CCA_Ports \
    Core_Parallel Core_Math Core_Disclosure Core_Util Core_Thread Core_Persistent Core_Exceptions Core_Containers \
    Core_Malloc Core_IO Core_OS                             
CORE_STATIC_LIBS := \
    $(TEEM_LIBRARY) $(XML2_LIBRARY) $(Z_LIBRARY) $(PETSC_LIBRARY) $(HYPRE_LIBRARY) \
    $(LAPACK_LIBRARY) $(BLAS_LIBRARY) $(F_LIBRARY) $(MPI_LIBRARY) $(THREAD_LIBRARY) $(X_LIBRARY) $(M_LIBRARY) \
    $(DL_LIBRARY) $(CUDA_LIBRARY) $(MAGMA_LIBRARY) $(GPERFTOOLS_LIBRARY)

################################################################
# Auto dependency generation flags
#

CC_DEPEND_MODE	    := normal
CC_DEPEND_REGEN      = -MD
CC_DEPEND_EXT       := d
F77_DEPEND_MODE      = normal
F77_DEPEND_REGEN     = -MD
F77_DEPEND_EXT	    := d

NEED_SONAME  := yes

# Echo (with no new line)
ECHO_NNL     := echo -n

ifeq ($(REPOSITORY_FLAGS),)
  repository = 
else
  repository = $(REPOSITORY_FLAGS) $(patsubst %:$(1),%,$(filter %:$(1),$(ALL_LIB_ASSOCIATIONS)))
endif

ifeq ($(NEED_SONAME),yes)
  SONAMEFLAG = -Wl,-soname,$(notdir $@)
else
  SONAMEFLAG = 
endif

.SUFFIXES: .cc .cu .c .$(OBJEXT) .s .F .f .cpp .cxx

.cc.$(OBJEXT): $<
ifeq ($(SCI_MAKE_BE_QUIET),true)
	@-rm -f $@
else
	-rm -f $@
endif
ifeq ($(CC_DEPEND_MODE),normal)
  ifneq ($(TAU_MAKEFILE),)
    ifneq ($(PDTDIR),)
	-echo "Building $*.$(OBJEXT) ..." ; \
	pdbfile=`basename $< .cc`.pdb ; \
	rm $$pdbfile ; \
	$(PDTPARSE) $< $(TAU_INCLUDE) $(TAU_MPI_INCLUDE) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -DSCI_Hash_Map_h -DSCI_HASH_SET_H -DPDT_PARSER; \
	rm $*.inst.cc ; \
	$(TAUINSTR) $$pdbfile $< -o $*.inst.cc -f $(SRCTOP)/tau/select.dat ; \
        $(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c -I`dirname $<` $*.inst.cc -o $@
	@if [ -f $@ ] ; then  \
	echo " *** Instrumented $*.$(OBJEXT) successfully  " ; \
	else \
	echo " *** Failed to instrumented $*.$(OBJEXT)  " ; \
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ ; \
	ls -l $@; \
	fi ; \
	rm -f $$pdbfile ; 
    else
      ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ ; 
      else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ ; 
      endif
    endif
  else
    ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
    else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
    endif
  endif
else
  ifeq ($(CC_DEPEND_MODE),modify)
    ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT).tmp && mv -f $(basename $@).$(CC_DEPEND_EXT).tmp $(basename $@).$(CC_DEPEND_EXT)
    else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT).tmp && mv -f $(basename $@).$(CC_DEPEND_EXT).tmp $(basename $@).$(CC_DEPEND_EXT)
    endif
  else
    ifeq ($(CC_DEPEND_MODE),move)
      ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
	@sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	@rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
      else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
	sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
      endif
    else
      ifeq ($(CC_DEPEND_MODE),modify_and_move)
        ifeq ($(SCI_MAKE_BE_QUIET),true)
	  @echo "Compiling: $<"
	  @$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && \
             $(SED) -e 's,^$(notdir $@),$@,' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	  @rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
        else
	  $(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && \
             $(SED) -e 's,^$(notdir $@),$@,' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	  rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
        endif
      else
        ifeq ($(CC_DEPEND_MODE),stdout)
          #
          # .d - most likely gcc... have to explicitly put it in a .d file.
          #
          # -M outputs to stdout(err?) dependency info.  Redirect this output
          # to a temp file.  Prepend that file to replace filename.o with:
          # <path_to_file>/filename.o.  Then remove the temp file.
          #
          ifeq ($(SCI_MAKE_BE_QUIET),true)
	    @echo "Compiling: $<"
	    @rm -f $(basename $@).$(CC_DEPEND_EXT)
	    @$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)

            # Now do the actual compile!
	    @$(CXX) $(CXXFLAGS) $(INCLUDES) $(call repository,$@) -c $< -o $@
          else
	    @echo "CREATING DEPENDENCY INFORMATION:"
	    @rm -f $(basename $@).$(CC_DEPEND_EXT)
	    $(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)

            # Now do the actual compile!
	    @echo "ACTUALLY COMPILING:"
	    $(CXX) $(CXXFLAGS) $(INCLUDES) $(call repository,$@) -c $< -o $@
          endif
        else
	  @echo ".cc rule: Unknown CC_DEPEND_MODE: $(CC_DEPEND_MODE)"
	  @exit 1
        endif
      endif
    endif
  endif
endif

.cu.$(OBJEXT): $<
ifeq ($(SCI_MAKE_BE_QUIET),true)
	@-rm -f $@
else
	-rm -f $@
endif
#
# .d - most likely gcc... have to explicitly put it in a .d file.
#
# -M outputs to stdout(err?) dependency info.
# Prepend that file to replace filename.o with:
#     <path_to_file>/filename.o.
# Note: For the CUDA compiler (nvcc), it is necessary to generate
#       dependencies after the compilation phase.
#
ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(NVCC) $(NVCC_CXXFLAGS) $(INCLUDES) -Xcompiler $(CC_DEPEND_REGEN) $(call repository,$@) -c -dc $< -o $@
	@rm -f $(basename $@).$(CC_DEPEND_EXT)
	@$(NVCC) $(NVCC_CXXFLAGS) $(INCLUDES) -M $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)
else
	$(NVCC) $(NVCC_CXXFLAGS) $(INCLUDES) -Xcompiler $(CC_DEPEND_REGEN) $(call repository,$@) -c -dc $< -o $@
	@rm -f $(basename $@).$(CC_DEPEND_EXT)
	$(NVCC) $(NVCC_CXXFLAGS) $(INCLUDES) -M $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)
endif

.cpp.$(OBJEXT): $<
	-rm -f $@
ifeq ($(CC_DEPEND_MODE),normal)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
else
  ifeq ($(CC_DEPEND_MODE),modify)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT).tmp && mv -f $(basename $@).$(CC_DEPEND_EXT).tmp $(basename $@).$(CC_DEPEND_EXT)
  else
    ifeq ($(CC_DEPEND_MODE),move)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
	sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
    else
      ifeq ($(CC_DEPEND_MODE),stdout)
        #
        # .d - most likely gcc... have to explicitly put it in a .d file.
        #
        # -M outputs to stdout(err?) dependency info.  Redirect this output
        # to a temp file.  Awk that file to replace filename.o with:
        # <path_to_file>/filename.o.  Then remove the temp file.
        #
	@echo "CREATING DEPENDENCY INFORMATION:"
	@rm -f $(basename $@).$(CC_DEPEND_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)

        # Now do the actual compile!
	@echo "ACTUALLY COMPILING:"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(call repository,$@) -c $< -o $@
      else
	@echo ".cpp rule: Unknown CC_DEPEND_MODE: $(CC_DEPEND_MODE)"
	@exit 1
      endif
    endif
  endif
endif

.F.$(OBJEXT): $<
ifeq ($(SCI_MAKE_BE_QUIET),true)
	@-rm -f $@
else
	-rm -f $@
endif
ifeq ($(F77_DEPEND_MODE),normal)
  ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Fortran:   $<" 
	@$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@
  else
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@
  endif
else
  ifeq ($(F77_DEPEND_MODE),modify)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@ && if test -f $(basename $@).$(F77_DEPEND_EXT); then $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT).tmp && mv -f $(basename $@).$(F77_DEPEND_EXT).tmp $(basename $@).$(F77_DEPEND_EXT); fi
  else
    ifeq ($(F77_DEPEND_MODE),move)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@
	sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT)
	rm $(basename $(notdir $@)).$(F77_DEPEND_EXT)
    else
      ifeq ($(F77_DEPEND_MODE),modify_and_move)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c -o $@ $< && \
             $(SED) -e 's,^$(notdir $@),$@,' $(basename $(notdir $@)).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT)
	@rm $(basename $(notdir $@)).$(F77_DEPEND_EXT)
      else
        ifeq ($(F77_DEPEND_MODE),stdout)
          #
          # .d - most likely gcc... have to explicitly put it in a .d file.
          #
          # -M outputs to stdout(err?) dependency info.  Redirect this output
          # to a temp file.  Awk that file to replace filename.o with:
          # <path_to_file>/filename.o.  Then remove the temp file.
          #
	  @rm -f $(basename $@).$(F77_DEPEND_EXT)
          ifeq ($(SCI_MAKE_BE_QUIET),true)
            # This just generates the .d dependency file:
	    @$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(F77_DEPEND_EXT)

            # The following if/grep/sed is necessary to fix the .d file for the PGI compiler on Ranger:
	    @if `grep -q "^../src/" $(basename $@).$(F77_DEPEND_EXT)`; then sed 's,^../src/,,' $(basename $@).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT).tmp; mv $(basename $@).$(F77_DEPEND_EXT).tmp $(basename $@).$(F77_DEPEND_EXT) ; fi

            # Now do the actual compile!
	    @echo "Fortran:   $<"
	    @$(F77) $(FFLAGS) $(INCLUDES) -c $< -o $@
          else
	    @echo "CREATING DEPENDENCY INFORMATION:"
	    $(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(F77_DEPEND_EXT)

            # The following if/grep/sed is necessary for PGI compiler on Ranger:
	    @if `grep -q "^../src/" $(basename $@).$(F77_DEPEND_EXT)`; then sed 's,^../src/,,' $(basename $@).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT).tmp; mv $(basename $@).$(F77_DEPEND_EXT).tmp $(basename $@).$(F77_DEPEND_EXT) ; fi

            # Now do the actual compile!
	    @echo "ACTUALLY COMPILING:"
	    $(F77) $(FFLAGS) $(INCLUDES) -c $< -o $@
          endif
        else
	  @echo "Unknown F77_DEPEND_MODE: $(F77_DEPEND_MODE) (for $@)"
	  @exit 1
        endif
      endif
    endif
  endif
endif

.f.$(OBJEXT): $<
	-rm -f $@
ifeq ($(F77_DEPEND_MODE),normal)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@
else
  ifeq ($(F77_DEPEND_MODE),modify)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@ && if test -f $(basename $@).$(F77_DEPEND_EXT); then $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT).tmp && mv -f $(basename $@).$(F77_DEPEND_EXT).tmp $(basename $@).$(F77_DEPEND_EXT); fi
  else
    ifeq ($(F77_DEPEND_MODE),move)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< -o $@
	sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(F77_DEPEND_EXT) > $(basename $@).$(F77_DEPEND_EXT)
	rm $(basename $(notdir $@)).$(F77_DEPEND_EXT)
    else
      ifeq ($(F77_DEPEND_MODE),stdout)
        #
        # .d - most likely gcc... have to explicitly put it in a .d file.
        #
        # -M outputs to stdout(err?) dependency info.  Redirect this output
        # to a temp file.  Awk that file to replace filename.o with:
        # <path_to_file>/filename.o.  Then remove the temp file.
        #
	@echo "CREATING DEPENDENCY INFORMATION:"
	@rm -f $(basename $@).$(F77_DEPEND_EXT)
	$(F77) $(FFLAGS) $(FINCLUDES) $(F77_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(F77_DEPEND_EXT)

        # Now do the actual compile!
	@echo "ACTUALLY COMPILING:"
	$(F77) $(FFLAGS) $(INCLUDES) -c $< -o $@
      else
	@$echo "Unknown F77_DEPEND_MODE: $(F77_DEPEND_MODE) (for $@)"
	@exit 1
      endif
    endif
  endif
endif

.c.$(OBJEXT): $<
ifeq ($(SCI_MAKE_BE_QUIET),true)
	@-rm -f $@
else
	-rm -f $@
endif
ifeq ($(CC_DEPEND_MODE),normal)
  ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< -o $@
  else
	$(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< -o $@
  endif
else
  ifeq ($(CC_DEPEND_MODE),modify)
	$(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< -o $@ && $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT).tmp && mv -f $(basename $@).$(CC_DEPEND_EXT).tmp $(basename $@).$(CC_DEPEND_EXT)
  else
    ifeq ($(CC_DEPEND_MODE),move)
	$(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< -o $@
	sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
    else
      ifeq ($(CC_DEPEND_MODE),modify_and_move)
        ifeq ($(SCI_MAKE_BE_QUIET),true)
	  @echo "Compiling: $<"
	  @$(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && \
             $(SED) -e 's,^$(notdir $@),$@,' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	  @rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
        else
	  $(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && \
             $(SED) -e 's,^$(notdir $@),$@,' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	  rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
        endif
      else
        ifeq ($(CC_DEPEND_MODE),stdout)
          #
          # .d - most likely gcc... have to explicitly put it in a .d file.
          #
          # -M outputs to stdout(err?) dependency info.  Redirect this output
          # to a temp file.  Awk that file to replace filename.o with:
          # <path_to_file>/filename.o.  Then remove the temp file.
          #
	  @echo "CREATING DEPENDENCY INFORMATION:"
	  @rm -f $(basename $@).$(CC_DEPEND_EXT)
	  $(CC) $(CFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)
	  @rm -f $(basename $@).depend

          # Now do the actual compile!
	  @echo "ACTUALLY COMPILING:"
	  $(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
        else
	  @echo ".c rule: Unknown CC_DEPEND_MODE: $(CC_DEPEND_MODE)"
	  @exit 1
        endif
      endif
    endif
  endif
endif

.s.$(OBJEXT): $<
	$(AS) $(ASFLAGS) -o $@ $< -o $@

.cxx.o: $<
ifeq ($(SCI_MAKE_BE_QUIET),true)
	@-rm -f $@
else
	-rm -f $@
endif
ifeq ($(CC_DEPEND_MODE),normal)
  ifeq ($(SCI_MAKE_BE_QUIET),true)
	@echo "Compiling: $<"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ ;
  else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ ;
  endif
else
  ifeq ($(CC_DEPEND_MODE),modify)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@ && $(SED) -e 's,^$(notdir $@),$@,' $(basename $@).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT).tmp && mv -f $(basename $@).$(CC_DEPEND_EXT).tmp $(basename $@).$(CC_DEPEND_EXT)
  else
    ifeq ($(CC_DEPEND_MODE),move)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) $(call repository,$@) -c $< -o $@
	sed 's,^$(basename $@),$@,g' $(basename $(notdir $@)).$(CC_DEPEND_EXT) > $(basename $@).$(CC_DEPEND_EXT)
	rm $(basename $(notdir $@)).$(CC_DEPEND_EXT)
    else
      ifeq ($(CC_DEPEND_MODE),stdout)
        #
        # .d - most likely gcc... have to explicitly put it in a .d file.
        #
        # -M outputs to stdout(err?) dependency info.  Redirect this output
        # to a temp file.  Prepend that file to replace filename.o with:
        # <path_to_file>/filename.o.  Then remove the temp file.
        #
	@echo "CREATING DEPENDENCY INFORMATION:"
	@rm -f $(basename $@).$(CC_DEPEND_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CC_DEPEND_REGEN) -c $< | sed 's,^$(notdir $@),$@,g' > $(basename $@).$(CC_DEPEND_EXT)

        # Now do the actual compile!
	@echo "ACTUALLY COMPILING:"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(call repository,$@) -c $< -o $@
      else
	@echo ".cxx rule: Unknown CC_DEPEND_MODE: $(CC_DEPEND_MODE)"
	@exit 1
      endif
    endif
  endif
endif

JAVAC := @JAVAC@
JAR := @JAR@
#ifeq ($(HAVE_JAVA),yes)
# .java.class:
#$(JAVAC) -classpath ...
#endif
