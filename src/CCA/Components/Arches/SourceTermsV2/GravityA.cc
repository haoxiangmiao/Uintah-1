#include <CCA/Components/Arches/SourceTermsV2/GravityA.h>
#include <CCA/Components/Arches/UPSHelper.h>


namespace Uintah{

//--------------------------------------------------------------------------------------------------
GravityA::GravityA( std::string task_name, int matl_index ) :
TaskInterface( task_name, matl_index ) 
{}

//--------------------------------------------------------------------------------------------------
GravityA::~GravityA()
{}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace GravityA::loadTaskComputeBCsFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::BC>( this
                                     , &GravityA::compute_bcs<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     //, &GravityA::compute_bcs<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &GravityA::compute_bcs<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace GravityA::loadTaskInitializeFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::INITIALIZE>( this
                                     , &GravityA::initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &GravityA::initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &GravityA::initialize<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace GravityA::loadTaskEvalFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_EVAL>( this
                                     , &GravityA::eval<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &GravityA::eval<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &GravityA::eval<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

TaskAssignedExecutionSpace GravityA::loadTaskTimestepInitFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_INITIALIZE>( this
                                     , &GravityA::timestep_init<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &GravityA::timestep_init<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     );
}

TaskAssignedExecutionSpace GravityA::loadTaskRestartInitFunctionPointers()
{
 return  TaskAssignedExecutionSpace::NONE_EXECUTION_SPACE;
}

//--------------------------------------------------------------------------------------------------
void
GravityA::problemSetup( ProblemSpecP& db ){

  // check for gravity
  const ProblemSpecP params_root = db->getRootNode();
  if (params_root->findBlock("PhysicalConstants")) {
    ProblemSpecP db_phys = params_root->findBlock("PhysicalConstants");
    db_phys->require("gravity", m_gravity);
  } else {
    throw InvalidValue("Error: Missing <PhysicalConstants> section in input file required for gravity",__FILE__,__LINE__);
  }
  m_gx_label             = _task_name+"_x";
  m_gy_label             = _task_name+"_y";
  m_gz_label             = _task_name+"_z";

  db->findBlock("reference_density")->getAttribute("value", m_ref_density);
  using namespace ArchesCore;
  m_density_label = parse_ups_for_role( DENSITY, db, "density" );
  
 
}

//--------------------------------------------------------------------------------------------------
void
GravityA::create_local_labels(){

  if (m_gravity[0] != 0.0) {
    register_new_variable<SFCXVariable<double> >( m_gx_label);
  } else if (m_gravity[1] != 0.0) {
    register_new_variable<SFCYVariable<double> >( m_gy_label);
  } else if (m_gravity[2] != 0.0) {
    register_new_variable<SFCZVariable<double> >( m_gz_label);
  }

}

//--------------------------------------------------------------------------------------------------
void
GravityA::register_initialize( std::vector<ArchesFieldContainer::VariableInformation>&
                                variable_registry, const bool packed_tasks ){

  if (m_gravity[0] != 0.0) {
     register_variable( m_gx_label,             ArchesFieldContainer::COMPUTES, variable_registry );
  } else if (m_gravity[1] != 0.0) {
     register_variable( m_gy_label,             ArchesFieldContainer::COMPUTES, variable_registry );
  } else if (m_gravity[2] != 0.0) {
     register_variable( m_gz_label,             ArchesFieldContainer::COMPUTES, variable_registry );
  }
  register_variable( m_density_label, ArchesFieldContainer::REQUIRES, 1 ,ArchesFieldContainer::NEWDW,
                    variable_registry );

}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemorySpace>
void GravityA::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemorySpace>& executionObject ){

  constCCVariable<double>& density  = tsk_info->get_const_uintah_field_add<constCCVariable<double > >( m_density_label );
  Uintah::BlockRange range(patch->getCellLowIndex(), patch->getCellHighIndex() );


  if (m_gravity[0] != 0.0) {
    SFCXVariable<double>& gx = tsk_info->get_uintah_field_add<SFCXVariable<double> >(m_gx_label);
    gx.initialize(0.0);

    Uintah::parallel_for( range, [&](int i, int j, int k){
      gx(i,j,k) = (0.5*(density(i,j,k) + density(i-1,j,k)) - m_ref_density )*m_gravity[0];    
    });

  } else if (m_gravity[1] != 0.0) {
    SFCYVariable<double>& gy = tsk_info->get_uintah_field_add<SFCYVariable<double> >(m_gy_label);
    gy.initialize(0.0);

    Uintah::parallel_for( range, [&](int i, int j, int k){
      gy(i,j,k) = (0.5*(density(i,j,k) + density(i,j-1,k)) - m_ref_density )*m_gravity[1];    
    });

  } else if (m_gravity[2] != 0.0) {
    SFCZVariable<double>& gz = tsk_info->get_uintah_field_add<SFCZVariable<double> >(m_gz_label);
    gz.initialize(0.0);

    Uintah::parallel_for( range, [&](int i, int j, int k){
      gz(i,j,k) = (0.5*(density(i,j,k) + density(i,j,k-1)) - m_ref_density )*m_gravity[2];    
    });

  }



}
//--------------------------------------------------------------------------------------------------
void
GravityA::register_timestep_init( std::vector<ArchesFieldContainer::VariableInformation>&
                                   variable_registry , const bool packed_tasks){
}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void
GravityA::timestep_init( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& executionObject ){

}

//--------------------------------------------------------------------------------------------------
void
GravityA::register_timestep_eval( std::vector<ArchesFieldContainer::VariableInformation>&
                                   variable_registry, const int time_substep , const bool packed_tasks){

  if (m_gravity[0] != 0.0) {
     register_variable( m_gx_label,             ArchesFieldContainer::COMPUTES, variable_registry, time_substep  );
  } else if (m_gravity[1] != 0.0) {
     register_variable( m_gy_label,             ArchesFieldContainer::COMPUTES, variable_registry, time_substep  );
  } else if (m_gravity[2] != 0.0) {
     register_variable( m_gz_label,             ArchesFieldContainer::COMPUTES, variable_registry, time_substep  );
  }
  register_variable( m_density_label, ArchesFieldContainer::REQUIRES, 1 ,ArchesFieldContainer::NEWDW,
                    variable_registry, time_substep  );


}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemorySpace>
void GravityA::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemorySpace>& executionObject ){

  constCCVariable<double>& density  = tsk_info->get_const_uintah_field_add<constCCVariable<double > >( m_density_label );
  Uintah::BlockRange range(patch->getCellLowIndex(), patch->getCellHighIndex() );


  if (m_gravity[0] != 0.0) {
    SFCXVariable<double>& gx = tsk_info->get_uintah_field_add<SFCXVariable<double> >(m_gx_label);
    gx.initialize(0.0);

    Uintah::parallel_for( range, [&](int i, int j, int k){
      gx(i,j,k) = (0.5*(density(i,j,k) + density(i-1,j,k)) - m_ref_density )*m_gravity[0];    
    });

  } else if (m_gravity[1] != 0.0) {
    SFCYVariable<double>& gy = tsk_info->get_uintah_field_add<SFCYVariable<double> >(m_gy_label);
    gy.initialize(0.0);

    Uintah::parallel_for( range, [&](int i, int j, int k){
      gy(i,j,k) = (0.5*(density(i,j,k) + density(i,j-1,k)) - m_ref_density )*m_gravity[1];    
    });

  } else if (m_gravity[2] != 0.0) {
    SFCZVariable<double>& gz = tsk_info->get_uintah_field_add<SFCZVariable<double> >(m_gz_label);
    gz.initialize(0.0);

    Uintah::parallel_for( range, [&](int i, int j, int k){
      gz(i,j,k) = (0.5*(density(i,j,k) + density(i,j,k-1)) - m_ref_density )*m_gravity[2];    
    });

  }

}


} //namespace Uintah
