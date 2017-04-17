#include <CCA/Components/Arches/PropertyModelsV2/sumRadiation.h>
#include <CCA/Components/Arches/ChemMix/ChemHelper.h>
#include <ostream>
#include <cmath>
namespace Uintah{

//--------------------------------------------------------------------------------------------------
void
sumRadiation::problemSetup( ProblemSpecP& db ){

  ProblemSpecP db_prop = db;

  bool foundGas=false;
  //bool foundPart=false;  // intended to be used in the future
  for ( ProblemSpecP db_model = db->findBlock("model"); db_model != nullptr; db_model=db_model->findNextBlock("model") ){

    std::string type;
    db_model->getAttribute("type", type);

    if ( type == "gasRadProperties" ){
      std::string fieldName;
      db_model->getAttribute("label",fieldName); 
      _gas_part_name.push_back(fieldName);
      foundGas=true;
    } else if ( type == "partRadProperties" ) {
      std::string fieldName;
      db_model->getAttribute("label",fieldName); 
      _gas_part_name.push_back(fieldName);
      //foundPart=true;
    }
  }

  if (foundGas==false){
    ChemHelper& helper = ChemHelper::self();
    helper.add_lookup_species("abskg");
  }

//----------------------set name of total absorption coefficient ------------//
  bool foundName=false;
  ProblemSpecP db_source = db_prop->getRootNode()->findBlock("CFD")->findBlock("ARCHES")->findBlock("TransportEqns")->findBlock("Sources") ;
  for ( ProblemSpecP db_src = db_source->findBlock("src"); db_src != nullptr; db_src = db_src->findNextBlock("src")){
    std::string radiation_model;
    db_src->getAttribute("type", radiation_model);
    if (radiation_model == "do_radiation" || radiation_model== "rmcrt_radiation"){
      if (foundName == false){
        if (db_src->findBlock("abskt")){
          db_src->findBlock("abskt")->getAttribute("label",m_abskt_name);
          foundName=true;
          //--------Now check if scattering is on for DO----//
          if(radiation_model == "do_radiation"){
            bool scatteringOn=false;
                           
            db_src->findBlock("DORadiationModel")->getWithDefault("ScatteringOn",scatteringOn,false) ;
            if (scatteringOn){
              _gas_part_name.push_back("scatkt");
            }
          //------------------------------------------------//
          }
        }else{
          throw ProblemSetupException("Absorption coefficient not specified.",__FILE__, __LINE__);
        }
      }else{
        std::string checkString;
        db_src->findBlock("abskg")->getAttribute("label",checkString);
        if (checkString != m_abskt_name){
          throw ProblemSetupException("Error: Multiple Radiation solvers detected, but they are using different absorption coefficients, which is not supported. ",__FILE__, __LINE__);
        }
      }
    }
  }
//---------------------------------------------------------------------------//
}

//--------------------------------------------------------------------------------------------------
void
sumRadiation::create_local_labels(){
    register_new_variable<CCVariable<double> >(m_abskt_name);

}

//--------------------------------------------------------------------------------------------------
void
sumRadiation::register_initialize( VIVec& variable_registry ){

  register_variable( m_abskt_name, ArchesFieldContainer::COMPUTES, variable_registry );
  register_variable("volFraction" , ArchesFieldContainer::REQUIRES,0,ArchesFieldContainer::NEWDW,variable_registry);
  for (unsigned int i=0; i<_gas_part_name.size(); i++){
    register_variable(_gas_part_name[i] , Uintah::ArchesFieldContainer::REQUIRES, variable_registry);
  }

}

void
sumRadiation::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info ){



  CCVariable<double>& abskt = *(tsk_info->get_uintah_field<CCVariable<double> >(m_abskt_name));
  constCCVariable<double>&  volFrac = tsk_info->get_const_uintah_field_add<constCCVariable<double> >("volFraction");
  
  abskt.initialize(1.0);
  for (unsigned int i=0; i<_gas_part_name.size(); i++){
    constCCVariable<double>&  abskf = tsk_info->get_const_uintah_field_add<constCCVariable<double> >(_gas_part_name[i]);

  //Uintah::BlockRange range(patch->getExtraCellLowIndex(), patch->getExtraCellHighIndex());
  Uintah::BlockRange range(patch->getCellLowIndex(), patch->getCellHighIndex());
  Uintah::parallel_for( range, [&](int i, int j, int k){
  abskt(i,j,k)=(volFrac(i,j,k) > 1e-16) ? abskt(i,j,k)+abskf(i,j,k)-1.0/ (double) _gas_part_name.size()  : 1.0;
  });

  }


}

//--------------------------------------------------------------------------------------------------
void sumRadiation::register_restart_initialize( VIVec& variable_registry ){
  //register_initialize(variable_registry);
}

void sumRadiation::restart_initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info ){
  //initialize( patch,tsk_info);
}

//--------------------------------------------------------------------------------------------------
void sumRadiation::register_timestep_init( VIVec& variable_registry ){
  //register_initialize( variable_registry );
}

void sumRadiation::timestep_init( const Patch* patch, ArchesTaskInfoManager* tsk_info ){
  //initialize( patch,tsk_info);
}


void sumRadiation::register_timestep_eval( VIVec& variable_registry, const int time_substep ){
  register_initialize( variable_registry );
}

void
sumRadiation::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info ){
  initialize( patch,tsk_info);
}

} //namespace Uintah