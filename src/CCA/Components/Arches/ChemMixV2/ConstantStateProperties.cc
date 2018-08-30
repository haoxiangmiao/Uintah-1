#include <CCA/Components/Arches/ChemMixV2/ConstantStateProperties.h>

using namespace Uintah;

//--------------------------------------------------------------------------------------------------
ConstantStateProperties::ConstantStateProperties( std::string task_name, int matl_index ) :
TaskInterface( task_name, matl_index )
{}

//--------------------------------------------------------------------------------------------------
ConstantStateProperties::~ConstantStateProperties(){

}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace ConstantStateProperties::loadTaskComputeBCsFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::BC>( this
                                     , &ConstantStateProperties::compute_bcs<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     //, &ConstantStateProperties::compute_bcs<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &ConstantStateProperties::compute_bcs<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace ConstantStateProperties::loadTaskInitializeFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::INITIALIZE>( this
                                     , &ConstantStateProperties::initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     //, &ConstantStateProperties::initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &ConstantStateProperties::initialize<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
TaskAssignedExecutionSpace ConstantStateProperties::loadTaskEvalFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_EVAL>( this
                                     , &ConstantStateProperties::eval<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     //, &ConstantStateProperties::eval<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &ConstantStateProperties::eval<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

TaskAssignedExecutionSpace ConstantStateProperties::loadTaskTimestepInitFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::TIMESTEP_INITIALIZE>( this
                                     , &ConstantStateProperties::timestep_init<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &ConstantStateProperties::timestep_init<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     );
}

TaskAssignedExecutionSpace ConstantStateProperties::loadTaskRestartInitFunctionPointers()
{
  return create_portable_arches_tasks<TaskInterface::RESTART_INITIALIZE>( this
                                     , &ConstantStateProperties::restart_initialize<UINTAH_CPU_TAG>     // Task supports non-Kokkos builds
                                     , &ConstantStateProperties::restart_initialize<KOKKOS_OPENMP_TAG>  // Task supports Kokkos::OpenMP builds
                                     //, &ColdFlowProperties::timestep_init<KOKKOS_CUDA_TAG>    // Task supports Kokkos::Cuda builds
                                     );
}

//--------------------------------------------------------------------------------------------------
void ConstantStateProperties::problemSetup( ProblemSpecP& db ){

  for ( ProblemSpecP db_prop = db->findBlock("const_property");
	db_prop.get_rep() != nullptr;
        db_prop = db_prop->findNextBlock("const_property") ){

    std::string label;
    double value;

    db_prop->getAttribute("label", label);
    db_prop->getAttribute("value", value);

    m_name_to_value.insert( std::make_pair( label, value));

  }

}

//--------------------------------------------------------------------------------------------------
void ConstantStateProperties::create_local_labels(){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    register_new_variable<CCVariable<double> >( i->first );
  }

}

//--------------------------------------------------------------------------------------------------
void ConstantStateProperties::register_initialize( VIVec& variable_registry , const bool packed_tasks){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    register_variable( i->first, ArchesFieldContainer::COMPUTES, variable_registry );
  }

}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemorySpace>
void ConstantStateProperties::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemorySpace>& executionObject ){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    CCVariable<double>& var = tsk_info->get_uintah_field_add<CCVariable<double> >( i->first );
    var.initialize(i->second);
  }

}

//--------------------------------------------------------------------------------------------------
void ConstantStateProperties::register_timestep_init( VIVec& variable_registry , const bool packed_tasks){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    register_variable( i->first, ArchesFieldContainer::COMPUTES, variable_registry );
    register_variable( i->first, ArchesFieldContainer::REQUIRES, 0, ArchesFieldContainer::OLDDW, variable_registry );
  }

}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void ConstantStateProperties::timestep_init( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& executionObject ){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    constCCVariable<double>& old_var = tsk_info->get_const_uintah_field_add<constCCVariable<double> >( i->first );
    CCVariable<double>& var = tsk_info->get_uintah_field_add<CCVariable<double> >( i->first );

    var.copyData(old_var);
  }

}

//--------------------------------------------------------------------------------------------------
void ConstantStateProperties::register_restart_initialize( VIVec& variable_registry , const bool packed_tasks){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    register_variable( i->first, ArchesFieldContainer::COMPUTES, variable_registry );
  }

}

//--------------------------------------------------------------------------------------------------
template<typename ExecutionSpace, typename MemSpace>
void ConstantStateProperties::restart_initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemSpace>& executionObject ){

  for ( auto i = m_name_to_value.begin(); i != m_name_to_value.end(); i++ ){
    CCVariable<double>& var = tsk_info->get_uintah_field_add<CCVariable<double> >( i->first );
    var.initialize(i->second);
  }

}

void ConstantStateProperties::register_timestep_eval( VIVec& variable_registry, const int time_substep , const bool packed_tasks){}
void ConstantStateProperties::register_compute_bcs( VIVec& variable_registry, const int time_substep , const bool packed_tasks){}
template<typename ExecutionSpace, typename MemorySpace>
void ConstantStateProperties::compute_bcs( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemorySpace>& executionObject ){}
template<typename ExecutionSpace, typename MemorySpace>
void ConstantStateProperties::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info, ExecutionObject<ExecutionSpace, MemorySpace>& executionObject ){}
