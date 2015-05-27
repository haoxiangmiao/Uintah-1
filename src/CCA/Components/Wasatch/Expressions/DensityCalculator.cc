#include <CCA/Components/Wasatch/Expressions/DensityCalculator.h>

#include <sci_defs/uintah_defs.h>

#include <boost/foreach.hpp>

#define DGESV FIX_NAME(dgesv)

extern "C"{
  void DGESV( const int    * n,    // # of equations
              const int    * nrhs, // # of rhs to solve
                    double   A[],  // lhs matrix
              const int    * lda,  // leading dim of A (#eqn for us)
                    int    * ipiv, // integer work array
                    double   rhs[],// rhs array
              const int    * ldb,  // dimension of rhs (#eqn for us)
                    int    * info );
}

//-------------------------------------------------------------------

DensityCalculatorBase::
DensityCalculatorBase( const int neq,
                       const double rtol,
                       const size_t maxIter )
: neq_    ( neq     ),
  rtol_   ( rtol    ),
  maxIter_( maxIter )
{
  jac_ .resize( neq_*neq_ );
  res_ .resize( neq_      );
  ipiv_.resize( neq_      );
}

DensityCalculatorBase::~DensityCalculatorBase(){}

bool DensityCalculatorBase::solve( const DoubleVec& passThrough,
                                   DoubleVec& soln )
{
  unsigned niter = 0;
  double relErr = 0.0;

  do{
    calc_jacobian_and_res( passThrough, soln, jac_, res_ );
    switch( neq_ ){
      case 1: // avoid the overhead of the general equation solver for this trivial case.
        res_[0] /= jac_[0];  // put the displacement from Newton's method in the residual.
        break;
      default:
        // Solving J * delta = rhs  (note the missing minus sign, which is handled in the update below)
        // note that on entry, res_ is the rhs and on exit, it is the solution (delta).
        const int one=1; int info;
        DGESV( &neq_, &one, &jac_[0], &neq_, &ipiv_[0], &res_[0], &neq_, &info );
        if( info != 0 ){
          std::cout << "\nSOLVER FAILED: "<< info << "  " << soln[0] << ", " << soln[1] << std::endl;
        }
        assert( info==0 );
        break;
    } // switch
    relErr = 0.0;
    for( int i=0; i<neq_; ++i ){
      soln[i] -= res_[i];
      relErr += std::abs( res_[i]/get_normalization_factor(i) );
      // clip the solution to the valid range
      const std::pair<double,double>& bounds = get_bounds(i);
      soln[i] = std::max( std::min( bounds.second, soln[i] ), bounds.first );
    }
    ++niter;
  } while( relErr > rtol_ && niter < maxIter_ );
  return niter < maxIter_;
}


//===================================================================


template< typename FieldT >
DensFromMixfrac<FieldT>::
DensFromMixfrac( const InterpT& rhoEval,
                 const Expr::Tag& rhoFTag )
  : Expr::Expression<FieldT>(),
    DensityCalculatorBase( 1, 1e-6, 5 ),
    rhoEval_( rhoEval ),
    bounds_( rhoEval.get_bounds()[0] )
{
  this->set_gpu_runnable(false);
  
   rhoF_ = this->template create_field_request<FieldT>(rhoFTag);
}

//--------------------------------------------------------------------

template< typename FieldT >
DensFromMixfrac<FieldT>::
~DensFromMixfrac()
{}

//--------------------------------------------------------------------

template< typename FieldT >
void
DensFromMixfrac<FieldT>::
evaluate()
{
  
  typedef std::vector<FieldT*> SVolFieldVec;

  SVolFieldVec& results = this->get_value_vec();
  
  // jcs: can we do the linear solve in place? We probably can. If so,
  // we would only need one field, not two...
  FieldT& rho = *results[0];
  FieldT& badPts = *results[1];
  badPts <<= 0.0;

  const FieldT& rhoF = rhoF_->field_ref();
  typename FieldT::const_iterator irhoF = rhoF.begin();
  typename FieldT::iterator irho = rho.begin();
  typename FieldT::iterator ibad = badPts.begin();
  const typename FieldT::iterator irhoe = rho.end();
  size_t nbad = 0;
  DoubleVec soln(1), vals(1);
  for( ; irho!=irhoe; ++irho, ++irhoF, ++ibad){
    vals[0] = *irhoF;
    soln[0] = *irhoF / *irho;   // initial guess for the mixture fraction
    const bool converged = this->solve( vals, soln );  // soln contains the mixture fraction
    if( !converged ) {
      ++nbad;
      *ibad = 1.0;
    }
    *irho = *irhoF / soln[0];
  }
  if( nbad>0 ){
    std::cout << "\tConvergence failed at " << nbad << " points.\n";
  }
}

//--------------------------------------------------------------------

template<typename FieldT>
void
DensFromMixfrac<FieldT>::
calc_jacobian_and_res( const DensityCalculatorBase::DoubleVec& passThrough,
                       const DensityCalculatorBase::DoubleVec& soln,
                       DensityCalculatorBase::DoubleVec& jac,
                       DensityCalculatorBase::DoubleVec& res )
{
  const double rhoF = passThrough[0];
  const double& f = soln[0];
  const double rhoCalc = rhoEval_.value( &f );
  jac[0] = rhoCalc + f * rhoEval_.derivative( &f, 0 );
  res[0] = f * rhoCalc - rhoF;
}

//--------------------------------------------------------------------

template<typename FieldT>
double
DensFromMixfrac<FieldT>::get_normalization_factor( const unsigned i ) const
{
  return 0.5; // nominal value for mixture fraction
}

//--------------------------------------------------------------------

template<typename FieldT>
const std::pair<double,double>&
DensFromMixfrac<FieldT>::get_bounds( const unsigned i ) const
{
  return bounds_;
}

//--------------------------------------------------------------------

template< typename FieldT >
DensFromMixfrac<FieldT>::
Builder::Builder( const InterpT& rhoEval,
                  const Expr::TagList& resultsTag,
                  const Expr::Tag& rhoFTag  )
  : ExpressionBuilder( resultsTag ),
    rhoFTag_(rhoFTag),
    rhoEval_( rhoEval.clone() )
{
  if( resultsTag[0].context() != Expr::CARRY_FORWARD ){
    std::ostringstream msg;
    msg << "ERROR: Density must have CARRY_FORWARD context so that an initial guess is available\n\t"
        << __FILE__ << " : " << __LINE__ << std::endl;
    throw std::runtime_error( msg.str() );
  }
}

//--------------------------------------------------------------------

template< typename FieldT >
Expr::ExpressionBase*
DensFromMixfrac<FieldT>::
Builder::build() const
{
  return new DensFromMixfrac<FieldT>( *rhoEval_, rhoFTag_ );
}



//===================================================================

template< typename FieldT >
DensHeatLossMixfrac<FieldT>::
DensHeatLossMixfrac( const Expr::Tag& rhofTag,
                     const Expr::Tag& rhohTag,
                     const InterpT& densEvaluator,
                     const InterpT& enthEvaluator )
  : Expr::Expression<FieldT>(),
    DensityCalculatorBase( 2, 1e-6, 5 ),
    densEval_( densEvaluator ),
    enthEval_( enthEvaluator ),
    bounds_( densEvaluator.get_bounds() )
{
   rhof_ = this->template create_field_request<FieldT>(rhofTag);
   rhoh_ = this->template create_field_request<FieldT>(rhohTag);
}

//--------------------------------------------------------------------

template< typename FieldT >
DensHeatLossMixfrac<FieldT>::
~DensHeatLossMixfrac()
{}

//--------------------------------------------------------------------

template< typename FieldT >
void
DensHeatLossMixfrac<FieldT>::
evaluate()
{
  typename Expr::Expression<FieldT>::ValVec& result  = this->get_value_vec();
  FieldT& density = *result[0];
  FieldT& gamma   = *result[1];

  const FieldT& rhof = rhof_->field_ref();
  const FieldT& rhoh = rhoh_->field_ref();
  
  typename FieldT::const_iterator irhof = rhof.begin();
  typename FieldT::const_iterator irhoh = rhoh.begin();
  typename FieldT::iterator irho = density.begin();
  typename FieldT::iterator igam = gamma.begin();
  const typename FieldT::iterator irhoe = density.end();

  size_t nbad=0;
  DoubleVec soln(2), vals(2);
  for( ; irho!=irhoe; ++irho, ++igam, ++irhof, ++irhoh ){
    vals[0] = *irhof;
    vals[1] = *irhoh;
    soln[0] = *irhof / *irho; // mixture fraction
    soln[1] = *igam;          // heat loss
    const bool converged = this->solve( vals, soln );
    if( !converged ) ++nbad;
    *irho = *irhof / soln[0]; // set solution for density
    *igam = soln[1];          // heat loss
  }
  if( nbad>0 ){
    std::cout << "\tConvergence failed at " << nbad << " of " << density.window_with_ghost().local_npts() << " points.\n";
  }
}

//--------------------------------------------------------------------

template< typename FieldT >
void
DensHeatLossMixfrac<FieldT>::
calc_jacobian_and_res( const DensityCalculatorBase::DoubleVec& passThrough,
                       const DensityCalculatorBase::DoubleVec& soln,
                       DensityCalculatorBase::DoubleVec& jac,
                       DensityCalculatorBase::DoubleVec& res )
{
  const double& rhof = passThrough[0];
  const double& rhoh = passThrough[1];
  const double& f    = soln[0];
  //const double& gam  = soln[1];

  // if we hit the bounds on mixture fraction, we have a degenerate case,
  // so don't solve for heat loss in that situation.
  const double tol = 1e-4;
  const bool atBounds = ( std::abs(f-bounds_[0].first ) < tol ||
                          std::abs(f-bounds_[0].second) < tol );

  // evaluate density and enthalpy given the current guess for f and gamma.
  const double rho =                       densEval_.value( soln );
  const double h   = atBounds ? rhoh/rho : enthEval_.value( soln );
  // evaluate the residual function
  res[0] = f * rho - rhof;
  res[1] = atBounds ? 0 : rho * h - rhoh;

  // evaluate derivative for use in the jacobian matrix
  const double drhodf   =                densEval_.derivative( soln, 0 );
  const double drhodgam = atBounds ? 0 : densEval_.derivative( soln, 1 );
  const double dhdf     = atBounds ? 0 : enthEval_.derivative( soln, 0 );
  const double dhdgam   = atBounds ? 0 : enthEval_.derivative( soln, 1 );

  // strange ordering because of fortran/c conventions.
  jac[0] = rho + f * drhodf;
  jac[2] = atBounds ? 0 : f * drhodgam;
  jac[1] = atBounds ? 0 : rho*dhdf   + h*drhodf;
  jac[3] = atBounds ? 1 : rho*dhdgam + h*drhodgam;
}

//--------------------------------------------------------------------

template<typename FieldT>
double
DensHeatLossMixfrac<FieldT>::get_normalization_factor( const unsigned i ) const
{
  return 0.5; // nominal value for mixture fraction and heat loss (which range [0,1] and [-1,1] respectively).
}

//--------------------------------------------------------------------

template<typename FieldT>
const std::pair<double,double>&
DensHeatLossMixfrac<FieldT>::get_bounds( const unsigned i ) const
{
  return bounds_[i];
}

//--------------------------------------------------------------------

template< typename FieldT >
DensHeatLossMixfrac<FieldT>::
Builder::Builder( const Expr::Tag& rhoTag,
                  const Expr::Tag& gammaTag,
                  const Expr::Tag& rhofTag,
                  const Expr::Tag& rhohTag,
                  const InterpT& densEvaluator,
                  const InterpT& enthEvaluator )
  : ExpressionBuilder( tag_list(rhoTag,gammaTag) ),
    densEval_( densEvaluator.clone() ),
    enthEval_( enthEvaluator.clone() )
{
  if( rhoTag.context() != Expr::CARRY_FORWARD ){
    std::ostringstream msg;
    msg << "ERROR: Density must have CARRY_FORWARD context so that an initial guess is available\n\t"
        << __FILE__ << " : " << __LINE__ << std::endl;
    throw std::runtime_error( msg.str() );
  }
  if( gammaTag.context() != Expr::CARRY_FORWARD ){
    std::ostringstream msg;
    msg << "ERROR: Heat loss must have CARRY_FORWARD context so that an initial guess is available\n\t"
        << "specified tag: " << gammaTag << "\n\t"
        << __FILE__ << " : " << __LINE__ << std::endl;
    throw std::runtime_error( msg.str() );
  }
}

//--------------------------------------------------------------------

template< typename FieldT >
Expr::ExpressionBase*
DensHeatLossMixfrac<FieldT>::
Builder::build() const
{
  return new DensHeatLossMixfrac<FieldT>( rhofTag_,rhohTag_,*densEval_,*enthEval_ );
}

//====================================================================



template< typename FieldT >
TwoStreamMixingDensity<FieldT>::
TwoStreamMixingDensity( const Expr::Tag& rhofTag,
                        const double rho0,
                        const double rho1 )
  : Expr::Expression<FieldT>(),
    rho0_(rho0), rho1_(rho1),
    rhoMin_( rho0_ < rho1_ ? rho0_ : rho1),
    rhoMax_( rho0_ > rho1_ ? rho0_ : rho1)
{
  this->set_gpu_runnable(true);
  
   rhof_ = this->template create_field_request<FieldT>(rhofTag);
}

//--------------------------------------------------------------------

template< typename FieldT >
void
TwoStreamMixingDensity<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  FieldT& result = this->value();
  const FieldT& rf = rhof_->field_ref();
  const double tmp = (1/rho0_ - 1/rho1_);

  // first calculate the mixture fraction:
  result <<= (rf/rho0_) / (1.0+rf*tmp);
  
  // now use that to get the density:
  result <<= 1.0 / ( result/rho1_ + (1.0-result)/rho0_ );
  
  // repair bounds
  result <<= max ( min(result, rhoMax_), rhoMin_);
}

//--------------------------------------------------------------------

template< typename FieldT >
TwoStreamMixingDensity<FieldT>::
Builder::Builder( const Expr::Tag& resultTag,
                  const Expr::Tag& rhofTag,
                  const double rho0,
                  const double rho1 )
  : ExpressionBuilder( resultTag ),
    rho0_(rho0), rho1_(rho1),
    rhofTag_( rhofTag )
{}

//====================================================================


template< typename FieldT >
TwoStreamDensFromMixfr<FieldT>::
TwoStreamDensFromMixfr( const Expr::Tag& mixfrTag,
                        const double rho0,
                        const double rho1 )
  : Expr::Expression<FieldT>(),
    rho0_(rho0), rho1_(rho1),
    rhoMin_( rho0_ < rho1_ ? rho0_ : rho1),
    rhoMax_( rho0_ > rho1_ ? rho0_ : rho1)
{
  this->set_gpu_runnable(true);
  
   mixfr_ = this->template create_field_request<FieldT>(mixfrTag);
}

//--------------------------------------------------------------------

template< typename FieldT >
void
TwoStreamDensFromMixfr<FieldT>::
evaluate()
{
  using namespace SpatialOps;
  FieldT& result = this->value();
  const FieldT& f = mixfr_->field_ref();
  
  result <<= 1.0 / ( f/rho1_ + (1.0-f)/rho0_ );
  
  // repair bounds
  result <<= max ( min(result, rhoMax_), rhoMin_);
}

//--------------------------------------------------------------------

template< typename FieldT >
TwoStreamDensFromMixfr<FieldT>::
Builder::Builder( const Expr::Tag& resultTag,
                  const Expr::Tag& mixfrTag,
                  const double rho0,
                  const double rho1 )
  : ExpressionBuilder( resultTag ),
    rho0_(rho0), rho1_(rho1),
    mixfrTag_( mixfrTag )
{}

//====================================================================

// explicit template instantiation
#include <spatialops/structured/FVStaggeredFieldTypes.h>
template class DensFromMixfrac       <SpatialOps::SVolField>;
template class DensHeatLossMixfrac   <SpatialOps::SVolField>;
template class TwoStreamDensFromMixfr<SpatialOps::SVolField>;
template class TwoStreamMixingDensity<SpatialOps::SVolField>;