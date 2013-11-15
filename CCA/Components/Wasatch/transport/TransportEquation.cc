/**
 *  \file   TransportEquation.cc
 *  \date   Nov 13, 2013
 *  \author "James C. Sutherland"
 *
 *
 * The MIT License
 *
 * Copyright (c) 2013 The University of Utah
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
 *
 */



#include <CCA/Components/Wasatch/transport/TransportEquation.h>
#include <CCA/Components/Wasatch/Expressions/EmbeddedGeometry/EmbeddedGeometryHelper.h>

namespace Wasatch{

  //---------------------------------------------------------------------------

  TransportEquation::
  TransportEquation( GraphCategories& gc,
                     const std::string solnVarName,
                     Uintah::ProblemSpecP params,
                     const Direction stagLoc,
                     const bool isConstDensity,
                     const bool hasEmbeddedGeometry )
  : params_             ( params ),
    gc_                 ( gc ),
    solnVarName_        ( solnVarName ),
    solnVarTag_         ( solnVarName, Expr::STATE_N ),
    rhsTag_             ( solnVarName+"_rhs", Expr::STATE_NONE ),
    stagLoc_            ( stagLoc ),
    isConstDensity_     ( isConstDensity ),
    hasEmbeddedGeometry_(hasEmbeddedGeometry )
  {
//    setup();  // build all expressions required for this TransportEquation
  }

  //---------------------------------------------------------------------------

  void TransportEquation::setup()
  {
    FieldTagInfo tagInfo;
    Expr::TagList sourceTags;

    if( hasEmbeddedGeometry_ ){
      VolFractionNames& vNames = VolFractionNames::self();
      tagInfo[VOLUME_FRAC] = vNames.svol_frac_tag();
      tagInfo[AREA_FRAC_X] = vNames.xvol_frac_tag();
      tagInfo[AREA_FRAC_Y] = vNames.yvol_frac_tag();
      tagInfo[AREA_FRAC_Z] = vNames.zvol_frac_tag();
    }

    setup_diffusive_flux ( tagInfo );
    setup_convective_flux( tagInfo );
    setup_source_terms   ( tagInfo, sourceTags );

    // now build the RHS given the tagInfo that has been populated
    rhsExprID_ = setup_rhs( tagInfo, sourceTags );
    assert( rhsExprID_ != Expr::ExpressionID::null_id() );
    gc_[ADVANCE_SOLUTION]->rootIDs.insert( rhsExprID_ );
  }

  //---------------------------------------------------------------------------

  Expr::ExpressionID TransportEquation::get_rhs_id() const
  {
    assert( rhsExprID_ != Expr::ExpressionID::null_id() );
    return rhsExprID_;
  }

  //---------------------------------------------------------------------------

  std::string
  TransportEquation::dir_name() const
  {
    switch (stagLoc_) {
      case XDIR: return "x";
      case YDIR: return "y";
      case ZDIR: return "z";
      case NODIR:
      default: return "";
    }
  }

  //---------------------------------------------------------------------------

} // namespace Wasatch
