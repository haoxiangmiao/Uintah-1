#ifndef Uintah_Component_Arches_SSPInt_h
#define Uintah_Component_Arches_SSPInt_h

#include <CCA/Components/Arches/Task/TaskInterface.h>
#include <CCA/Components/Arches/Operators/Operators.h>
#include <spatialops/structured/FVStaggered.h>

namespace Uintah{ 

  template <typename T>
  class SSPInt : public TaskInterface { 

public: 

    SSPInt<T>( std::string task_name, int matl_index, std::vector<std::string> eqn_names ); 
    ~SSPInt<T>(); 

    /** @brief Input file interface **/ 
    void problemSetup( ProblemSpecP& db ); 

    /** @brief Build instruction for this class **/ 
    class Builder : public TaskInterface::TaskBuilder { 

      public: 

      Builder( std::string task_name, int matl_index, std::vector<std::string> eqn_names ) : 
        _task_name(task_name), _matl_index(matl_index), _eqn_names(eqn_names){}
      ~Builder(){}

      SSPInt* build()
      { return scinew SSPInt<T>( _task_name, _matl_index, _eqn_names ); }

      private: 

      std::string _task_name; 
      int _matl_index; 
      std::vector<std::string> _eqn_names; 

    };

protected: 

    void register_all_variables( std::vector<VariableInformation>& variable_registry, const int time_substep ); 

    void register_initialize( std::vector<VariableInformation>& variable_registry );
  

    void eval( const Patch* patch, FieldCollector* field_collector, 
               SpatialOps::OperatorDatabase& opr, 
               SchedToTaskInfo& info ); 

    void initialize( const Patch* patch, FieldCollector* field_collector, 
                     SpatialOps::OperatorDatabase& opr );

private:

    std::vector<std::string> _eqn_names; 
    Vector _ssp_beta, _ssp_alpha, _time_factor; 
    int _time_order; 
  
  };

  //Function definitions: 

  template <typename T>
  SSPInt<T>::SSPInt( std::string task_name, int matl_index, std::vector<std::string> eqn_names ) : 
  TaskInterface( task_name, matl_index ){

    // This needs to be done to set the variable type 
    // for this function. All templated tasks should do this. 
    set_type<T>(); 

    _eqn_names = eqn_names; 

    //coeffients: 

  
  }

  template <typename T>
  SSPInt<T>::~SSPInt()
  {
  }

  template <typename T>
  void SSPInt<T>::problemSetup( ProblemSpecP& db ){ 

    db->findBlock("TimeIntegrator")->getAttribute("order", _time_order); 

    if ( _time_order == 1 ){
      
      _ssp_alpha[0] = 0.0;
      _ssp_alpha[1] = 0.0;
      _ssp_alpha[2] = 0.0;

      _ssp_beta[0]  = 1.0;
      _ssp_beta[1]  = 0.0;
      _ssp_beta[2]  = 0.0;

      _time_factor[0] = 1.0;
      _time_factor[1] = 0.0;
      _time_factor[2] = 0.0; 

    } else if ( _time_order == 2 ) {

      _ssp_alpha[0]= 0.0;
      _ssp_alpha[1]= 0.5;
      _ssp_alpha[2]= 0.0;

      _ssp_beta[0]  = 1.0;
      _ssp_beta[1]  = 0.5;
      _ssp_beta[2]  = 0.0;

      _time_factor[0] = 1.0;
      _time_factor[1] = 1.0;
      _time_factor[2] = 0.0; 

    } else if ( _time_order == 3 ) {

      _ssp_alpha[0] = 0.0;
      _ssp_alpha[1] = 0.75;
      _ssp_alpha[2] = 1.0/3.0;

      _ssp_beta[0]  = 1.0;
      _ssp_beta[1]  = 0.25;
      _ssp_beta[2]  = 2.0/3.0;

      _time_factor[0] = 1.0;
      _time_factor[1] = 0.5;
      _time_factor[2] = 1.0; 

    } else {
      throw InvalidValue("Error: <TimeIntegrator> must have value: 1, 2, or 3 (representing the order).",__FILE__, __LINE__);             
    }

  }


  template <typename T>
  void SSPInt<T>::register_initialize( std::vector<VariableInformation>& variable_registry ){ 

  }
  
  //This is the work for the task.  First, get the variables. Second, do the work! 
  template <typename T> 
  void SSPInt<T>::initialize( const Patch* patch, FieldCollector* field_collector, 
                              SpatialOps::OperatorDatabase& opr ){ 
  }


  template <typename T> 
  void SSPInt<T>::register_all_variables( std::vector<VariableInformation>& variable_registry, const int time_substep ){
   
    //FUNCITON CALL     STRING NAME(VL)     TYPE       DEPENDENCY    GHOST DW     VR
    //register_variable( "templated_variable", _mytype, COMPUTES, 0, NEWDW, variable_registry, time_substep ); 
    typedef std::vector<std::string> SV;
    for ( SV::iterator i = _eqn_names.begin(); i != _eqn_names.end(); i++){ 
      register_variable( *i, _mytype, MODIFIES, 0, NEWDW, variable_registry, time_substep ); 
      register_variable( *i, _mytype, REQUIRES, 0, OLDDW, variable_registry, time_substep ); 
    }

    register_variable( "density", CC_DOUBLE, REQUIRES, 0, NEWDW, variable_registry, time_substep ); 
    register_variable( "density", CC_DOUBLE, REQUIRES, 0, OLDDW,  variable_registry, time_substep ); 
  
  }

  template <typename T>
  void SSPInt<T>::eval( const Patch* patch, FieldCollector* field_collector, 
                        SpatialOps::OperatorDatabase& opr, 
                        SchedToTaskInfo& info ){

    using namespace SpatialOps;
    using SpatialOps::operator *; 
    typedef SpatialOps::SVolField   SVolF;

    typedef std::vector<std::string> SV;
    for ( SV::iterator i = _eqn_names.begin(); i != _eqn_names.end(); i++){ 

      T* const phi     = field_collector->get_so_field<T>( *i, NEWDW        );
      T* const rho     = field_collector->get_so_field<T>( "density", NEWDW );

      //because of the ambiguity with the variables having the same name, we use the 
      //enums from the field collector to be specific about which variables we need. otherwise
      //we could/would end up with the wrong phi and density 
      T* const old_phi = field_collector->get_so_field<T>( *i, OLDDW );
      T* const old_rho = field_collector->get_so_field<T>( "density", OLDDW );

      double alpha = _ssp_alpha[info.time_substep]; 
      double beta  = _ssp_beta[info.time_substep];

      //Weighting: 
      *phi <<= ( alpha * ( *old_rho * *old_phi) + beta * ( *rho * *phi) ) / ( alpha * *old_rho + beta * *rho ); 

    }
  }
}
#endif 
