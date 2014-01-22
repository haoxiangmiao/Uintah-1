/*
 * The MIT License
 *
 * Copyright (c) 2012-2014 The University of Utah
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

#ifndef Wasatch_FieldAdaptor_h
#define Wasatch_FieldAdaptor_h

#include <spatialops/structured/FVStaggeredFieldTypes.h>
#include <spatialops/structured/MemoryWindow.h>

#include <Core/Grid/Variables/SFCXVariable.h>  /* x-face variable */
#include <Core/Grid/Variables/SFCYVariable.h>  /* y-face variable */
#include <Core/Grid/Variables/SFCZVariable.h>  /* z-face variable */
#include <Core/Grid/Variables/CCVariable.h>    /* cell variable   */
#include <Core/Grid/Variables/PerPatch.h>      /* single double per patch */
#include <Core/Disclosure/TypeDescription.h>
#include <sci_defs/uintah_defs.h>
#include <sci_defs/cuda_defs.h>


#include <CCA/Components/Wasatch/FieldTypes.h>


/**
 *  \file FieldAdaptor.h
 *
 *  \brief provides tools to translate between Uintah field types and
 *  SpatialOps-compatible field types.
 *
 *  This information is provided for the interface to the Expression
 *  library.  There should be no reason to use it otherwise.
 */

namespace Uintah{ class Patch; }

namespace Wasatch{

  /**
   *  \ingroup WasatchParser
   *  \enum FieldTypes
   *  \brief Enumerate the field types in Wasatch.
   */
  enum FieldTypes{
    SVOL, SSURFX, SSURFY, SSURFZ,
    XVOL, XSURFX, XSURFY, XSURFZ,
    YVOL, YSURFX, YSURFY, YSURFZ,
    ZVOL, ZSURFX, ZSURFY, ZSURFZ,
    PERPATCH
  };

  void get_bc_logicals( const Uintah::Patch* const patch,
                        SpatialOps::structured::IntVec& bcMinus,
                        SpatialOps::structured::IntVec& bcPlus );

  /**
   *  \ingroup WasatchFields
   *  \brief obtain the memory window for a uintah field that is to be wrapped as a SpatialOps field
   *  \param globSize - the full size of the parent field.
   *  \param patch - the patch that the field is associated with.
   *
   *  Note that if this signature is used, the field size will be deduced from the patch size as well
   *  as the field type traits.  For most use cases from Wasatch, the other signature should be used.
   */
  template< typename FieldT >
  SpatialOps::structured::MemoryWindow
  get_memory_window_for_uintah_field( const Uintah::Patch* const patch,
                                      const bool withoutGhost=false);

  /**
   *  \ingroup WasatchFields
   *
   *  \brief wrap a uintah field to obtain a SpatialOps field,
   *         returning a new pointer.  The caller is responsible for
   *         freeing the memory.
   *  \param uintahVar the uintah variable to wrap
   *  \param patch the patch that the field is associated with.
   *  \param mtype specifies the location for the field (GPU,CPU)
   *  \param deviceIndex in the case of a GPU field, this specifies which GPU it is on
   *  \param uintahDeviceVar for GPU fields, this is the pointer to the field on the device
   */
  template< typename FieldT, typename UFT >
  inline FieldT* wrap_uintah_field_as_spatialops( UFT& uintahVar,
                                                  const Uintah::Patch* const patch,
                                                  const SpatialOps::MemoryType mtype=SpatialOps::LOCAL_RAM,
                                                  const unsigned short int deviceIndex=0,
                                                  double* uintahDeviceVar = NULL )
  {
    /*
     * NOTE: before changing things here, look at the line:
     *    Uintah::OnDemandDataWarehouse::d_combineMemory = false;
     * in Wasatch.cc.  This is currently preventing Uintah from
     * combining patch memory.
     */
    namespace SS = SpatialOps::structured;
    using SCIRun::IntVector;

    const SCIRun::IntVector lowIx       = uintahVar.getLowIndex();
    const SCIRun::IntVector highIx      = uintahVar.getHighIndex();
    const SCIRun::IntVector fieldSize   = uintahVar.getWindow()->getData()->size();
    const SCIRun::IntVector fieldOffset = uintahVar.getWindow()->getOffset();
    const SCIRun::IntVector fieldExtent = highIx - lowIx;

    const SS::IntVec   size( fieldSize[0],   fieldSize[1],   fieldSize[2]   );
    const SS::IntVec extent( fieldExtent[0], fieldExtent[1], fieldExtent[2] );
    const SS::IntVec offset( lowIx[0]-fieldOffset[0], lowIx[1]-fieldOffset[1], lowIx[2]-fieldOffset[2] );

    SS::IntVec bcMinus, bcPlus;
    get_bc_logicals( patch, bcMinus, bcPlus );

    double* fieldValues_ = NULL;
    if( mtype == SpatialOps::EXTERNAL_CUDA_GPU ){
#     ifdef HAVE_CUDA
      fieldValues_ = const_cast<double*>( uintahDeviceVar );
#     endif
    } else{
      fieldValues_ = const_cast<typename FieldT::value_type*>( uintahVar.getPointer() );
    }

    return new FieldT( SpatialOps::structured::MemoryWindow( size, offset, extent ),
                       SS::BoundaryCellInfo::build<FieldT>(bcPlus),
                       SS::GhostData(1),  /* for now, we hard-code one ghost cell */
                       fieldValues_,
                       SpatialOps::structured::ExternalStorage,
                       mtype,
                       deviceIndex );
  }

  template<>
  inline SpatialOps::structured::SingleValueField*
  wrap_uintah_field_as_spatialops<SpatialOps::structured::SingleValueField,Uintah::PerPatch<double*> >(
      Uintah::PerPatch<double*>& uintahVar,
      const Uintah::Patch* const patch,
      const SpatialOps::MemoryType mtype,
      const unsigned short int deviceIndex,
      double* uintahDeviceVar );

  /**
   *  \ingroup WasatchParser
   *  \brief translate a string describing a field type to the FieldTypes enum.
   */
  FieldTypes get_field_type( std::string );

  /**
   *  \ingroup WasatchFields
   *  \struct SelectUintahFieldType
   *  \brief Convert SpatialOps field types to Uintah field types
   *
   *  This struct template provides two typedefs that define Uintah
   *  field types from SpatialOps field types:
   *   - \c type : the Uintah type
   *   - \c const_type : the Uintah const field type.
   */
  template<typename FieldT> struct SelectUintahFieldType;

  template<> struct SelectUintahFieldType<SpatialOps::structured::SingleValueField>{
    typedef Uintah::PerPatch<double> type;
    typedef Uintah::PerPatch<double> const_type;
  };

  template<> struct SelectUintahFieldType<int>{
    typedef Uintah::     CCVariable<int>  type;
    typedef Uintah::constCCVariable<int>  const_type;
  };

  template<> struct SelectUintahFieldType<SpatialOps::structured::SVolField>{
    typedef Uintah::     CCVariable<double>  type;
    typedef Uintah::constCCVariable<double>  const_type;
  };
  template<> struct SelectUintahFieldType<SpatialOps::structured::SSurfXField>{
    typedef Uintah::     SFCXVariable<double>  type;
    typedef Uintah::constSFCXVariable<double>  const_type;
  };
  template<> struct SelectUintahFieldType<SpatialOps::structured::SSurfYField>{
    typedef Uintah::     SFCYVariable<double>  type;
    typedef Uintah::constSFCYVariable<double>  const_type;
  };
  template<> struct SelectUintahFieldType<SpatialOps::structured::SSurfZField>{
    typedef Uintah::     SFCZVariable<double>  type;
    typedef Uintah::constSFCZVariable<double>  const_type;
  };

  template<> struct SelectUintahFieldType<SpatialOps::structured::XVolField>{
    typedef Uintah::     SFCXVariable<double>  type;
    typedef Uintah::constSFCXVariable<double>  const_type;
  };
  template<> struct SelectUintahFieldType<SpatialOps::structured::XSurfXField>{
    typedef Uintah::     CCVariable<double>  type;
    typedef Uintah::constCCVariable<double>  const_type;
  };
  template<> struct SelectUintahFieldType<SpatialOps::structured::XSurfYField>{
    typedef Uintah::     SFCYVariable<double>  type;
    typedef Uintah::constSFCYVariable<double>  const_type;
  };
  template<> struct SelectUintahFieldType<SpatialOps::structured::XSurfZField>{
    typedef Uintah::     SFCZVariable<double>  type;
    typedef Uintah::constSFCZVariable<double>  const_type;
  };

  template<> struct SelectUintahFieldType<SpatialOps::structured::YVolField>{
     typedef Uintah::     SFCYVariable<double>  type;
     typedef Uintah::constSFCYVariable<double>  const_type;
   };
   template<> struct SelectUintahFieldType<SpatialOps::structured::YSurfXField>{
     typedef Uintah::     SFCXVariable<double>  type;
     typedef Uintah::constSFCXVariable<double>  const_type;
   };
   template<> struct SelectUintahFieldType<SpatialOps::structured::YSurfYField>{
     typedef Uintah::     CCVariable<double>  type;
     typedef Uintah::constCCVariable<double>  const_type;
   };
   template<> struct SelectUintahFieldType<SpatialOps::structured::YSurfZField>{
     typedef Uintah::     SFCZVariable<double>  type;
     typedef Uintah::constSFCZVariable<double>  const_type;
   };

   template<> struct SelectUintahFieldType<SpatialOps::structured::ZVolField>{
     typedef Uintah::     SFCZVariable<double>  type;
     typedef Uintah::constSFCZVariable<double>  const_type;
   };
   template<> struct SelectUintahFieldType<SpatialOps::structured::ZSurfXField>{
     typedef Uintah::     SFCXVariable<double>  type;
     typedef Uintah::constSFCXVariable<double>  const_type;
   };
   template<> struct SelectUintahFieldType<SpatialOps::structured::ZSurfYField>{
     typedef Uintah::     SFCYVariable<double>  type;
     typedef Uintah::constSFCYVariable<double>  const_type;
   };
   template<> struct SelectUintahFieldType<SpatialOps::structured::ZSurfZField>{
     typedef Uintah::     CCVariable<double>  type;
     typedef Uintah::constCCVariable<double>  const_type;
   };

  /**
   *  \ingroup WasatchFields
   *  \brief Given the SpatialOps field type, this returns the
   *         Uintah::TypeDescription for the corresponding Uintah
   *         field type.
   */
  template<typename FieldT>
  inline const Uintah::TypeDescription* get_uintah_field_type_descriptor()
  {
    return SelectUintahFieldType<FieldT>::type::getTypeDescription();
  }

  /**
   *  \ingroup WasatchFields
   *  \brief Obtain the number of ghost cells for a given SpatialOps
   *         field type, assuming that there are the same number of
   *         ghost cells in each direction and on the (+) side as the
   *         (-) side.
   */
  template<typename FieldT> inline int get_n_ghost(){
    return 1;
  }

  template<> inline int get_n_ghost<SpatialOps::structured::SingleValueField>(){
    return 0;
  };

  /**
   *  \ingroup WasatchFields
   *  \brief Obtain the number of ghost cells in each direction for
   *         the given SpatialOps field type as a template parameter.
   *
   *  \return the Uintah::IntVector describing the number of ghost
   *          cells in each direction.
   */
  template<typename FieldT>
  inline Uintah::IntVector get_uintah_ghost_descriptor()
  {
    int ng = 0; // for extra cells
    return Uintah::IntVector(ng,ng,ng);
  }

  template<>
  inline Uintah::IntVector get_uintah_ghost_descriptor<SpatialOps::structured::SingleValueField>()
  {
    return Uintah::IntVector(0,0,0);
  }

  //====================================================================

  /**
   *  \ingroup WasatchFields
   *  \brief Given the SpatialOps field type as a template parameter,
   *         determine the Uintah GhostType information.
   *
   *  \return The Uintah::Ghost::GhostType for this field type.
   */
  template<typename FieldT> Uintah::Ghost::GhostType get_uintah_ghost_type();
}

#endif // Wasatch_FieldAdaptor_h
