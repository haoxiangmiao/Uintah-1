#ifndef __OPERATORS_VECTORFIELDOPERATOR_H__
#define __OPERATORS_VECTORFIELDOPERATOR_H__

#include "VectorOperatorFunctors.h"
#include <Core/GuiInterface/GuiVar.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/FieldPort.h>
#include <string>
#include <iostream>
using std::string;
using std::cerr;
using std::endl;


namespace Uintah {
using namespace SCIRun;

  class VectorFieldOperator: public Module {
  public:
    VectorFieldOperator(const string& id);
    virtual ~VectorFieldOperator() {}
    
    virtual void execute(void);
    
  private:
    template<class VectorField, class ScalarField>
     void performOperation(VectorField* vectorField, ScalarField* scalarField);
    template<class VectorField, class Field>
     void initField(VectorField* vectorField, Field* field);
    
    template<class VectorField, class ScalarField, class VectorOp >
     void computeScalars(VectorField* vectorField, ScalarField* scalarField,
			 VectorOp op /* TensorOp should be a functor for
					converting tensors scalars */ );

    //    TCLstring tcl_status;
    GuiInt guiOperation;

    FieldIPort *in;

    FieldOPort *sfout;
    //VectorFieldOPort *vfout;
  };

template<class VectorField, class ScalarField>
void VectorFieldOperator::performOperation(VectorField* vectorField,
					   ScalarField* scalarField)
{
  initField(vectorField, scalarField);

  switch(guiOperation.get()) {
  case 0: // extract element 1
  case 1: // extract element 2
  case 2: // extract element 3
    computeScalars(vectorField, scalarField,
		   VectorElementExtractionOp(guiOperation.get()));
  case 3: // Vector length
    computeScalars(vectorField, scalarField, LengthOp());
    break;
  default:
    std::cerr << "VectorFieldOperator::performOperation: "
	      << "Unexpected Operation Type #: " << guiOperation.get() << "\n";
  }
}

template<class VectorField, class ScalarField>
void VectorFieldOperator::initField(VectorField* vectorField,
				    ScalarField* field)
{
  ASSERT( vectorField->data_at() == Field::CELL ||
	  vectorField->data_at() == Field::NODE );

  typename VectorField::mesh_handle_type vmh = vectorField->get_typed_mesh();
  typename ScalarField::mesh_handle_type fmh = field->get_typed_mesh();
  BBox box;
  box = vmh->get_bounding_box();
  //resize the geometry
  fmh->set_nx(vmh->get_nx());
  fmh->set_ny(vmh->get_ny());
  fmh->set_nz(vmh->get_nz());
  fmh->set_min( box.min() );
  fmh->set_max( box.max() );
  //resize the data storage
  field->resize_fdata();

}

template<class VectorField, class ScalarField, class VectorOp >
void VectorFieldOperator::computeScalars(VectorField* vectorField,
					 ScalarField* scalarField,
					 VectorOp op 
					 /* VectorOp should be a functor for
					    converting vectors scalars */ )
{
  // so far only node and cell centered data
  ASSERT( vectorField->data_at() == Field::CELL ||
	  vectorField->data_at() == Field::NODE );


  typename VectorField::mesh_handle_type vmh = vectorField->get_typed_mesh();
  typename ScalarField::mesh_handle_type smh = scalarField->get_typed_mesh();

  if( vectorField->data_at() == Field::CELL){
    typename VectorField::mesh_type::cell_iterator v_it = vmh->cell_begin();
    typename VectorField::mesh_type::cell_iterator v_end = vmh->cell_end();
    typename ScalarField::mesh_type::cell_iterator s_it = smh->cell_begin();
    
    cerr<<"v_it = ("<<(*v_it).i_<<","<<(*v_it).j_<<","<<(*v_it).k_<<
      "), v_end = ("<<(*v_end).i_<<","<<(*v_end).j_<<","<<(*v_end).k_<<")\n";
    for( ; v_it != v_end; ++v_it, ++s_it){
      scalarField->fdata()[*s_it] = op(vectorField->fdata()[*v_it]);
    }
  } else {
    typename VectorField::mesh_type::node_iterator v_it = vmh->node_begin();
    typename VectorField::mesh_type::node_iterator v_end = vmh->node_end();
    typename ScalarField::mesh_type::node_iterator s_it = smh->node_begin();
  
    for( ; v_it != v_end; ++v_it, ++s_it){
      scalarField->fdata()[*s_it] = op(vectorField->fdata()[*v_it]);
    }
  }  
}

}

#endif // __OPERATORS_VECTORFIELDOPERATOR_H__

