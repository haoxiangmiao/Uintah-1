#ifndef Packages_Uintah_CCA_Components_Ice_LODI_h
#define Packages_Uintah_CCA_Components_Ice_LODI_h

#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Components/ICE/ICEMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/MPMMaterial.h>
#include <Packages/Uintah/CCA/Components/MPM/ConstitutiveModel/ConstitutiveModel.h>
#include <Packages/Uintah/Core/Grid/CCVariable.h>
#include <Packages/Uintah/Core/Grid/Patch.h>

#include <Packages/Uintah/Core/Grid/SimulationStateP.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/Core/Grid/CCVariable.h>
#include <Core/Containers/StaticArray.h>
#include <typeinfo>
using namespace Uintah;
namespace Uintah {
  
  //__________________________________
  // This struct contains the additional variables
  // required to apply the Lodi Temperature, density and velocity BC.
  struct Lodi_vars{                
    Lodi_vars() : di(6) {}
    constCCVariable<double> rho_old;     
    constCCVariable<double> temp_old; 
    constCCVariable<double> speedSound;    
    constCCVariable<Vector> vel_old;
    CCVariable<double> rho_CC;      // rho *after* BC has been applied
    CCVariable<Vector> vel_CC;      // vel *after* BC has been applied  
    CCVariable<double> press_tmp;        
    CCVariable<double> e;                
    CCVariable<Vector> nu;               
    StaticArray<CCVariable<Vector> > di; 
    double cv;
    double gamma; 
    double delT;
    bool setLodiBcs;                 
  };
  //__________________________________
  // This struct contains the additional variables
  // required to apply the Lodi pressure bcs.
  struct Lodi_vars_pressBC{
    Lodi_vars_pressBC(int numMatls): Temp_CC(numMatls), f_theta(numMatls) {}
    StaticArray<constCCVariable<double> > Temp_CC;
    StaticArray<constCCVariable<double> > f_theta;
    bool setLodiBcs;                      
  };

  void lodi_bc_preprocess( const Patch* patch,
                            Lodi_vars* lv,
                            ICELabel* lb,            
                            const int indx,
                            const vector<bool>& is_LODI_face,
                            DataWarehouse* old_dw,
                            DataWarehouse* new_dw);
                            
  void lodi_getVars_pressBC( const Patch* patch,
                             Lodi_vars_pressBC* lodi_vars,
                             ICELabel* lb,
                             SimulationStateP sharedState,
                             DataWarehouse* old_dw,
                             DataWarehouse* new_dw);                   
                            
                            
  void computeNu(CCVariable<Vector>& nu, 
                 const vector<bool>& is_LODI_face,
                 const CCVariable<double>& p, 
                 const Patch* patch);  

  void computeDi(StaticArray<CCVariable<Vector> >& d,
                 const vector<bool>& is_LODI_face,
                 constCCVariable<double>& rho_old,  
                 const CCVariable<double>& press_tmp, 
                 constCCVariable<Vector>& vel_old, 
                 constCCVariable<double>& speedSound, 
                 const Patch* patch);
                 
  double computeConvection(const double& nuFrt,     const double& nuMid, 
                           const double& nuLast,    const double& qFrt, 
                           const double& qMid,      const double& qLast,
                           const double& qConFrt,   const double& qConLast,
                           const double& deltaT,    const double& deltaX);

  void computeCornerCellIndices(const Patch* patch,
                                const Patch::FaceType face,
                                vector<IntVector>& crn);
  void getBoundaryEdges(const Patch* patch,
                        const Patch::FaceType face,
                        vector<Patch::FaceType>& face0);
                        
  int otherDirection(int dir1, int dir2);
  
  void FaceDensity_LODI(const Patch* patch,
                       const Patch::FaceType face,
                       CCVariable<double>& rho_CC,
                       Lodi_vars* lv,
                       const Vector& dx);
                  
  void FaceVel_LODI(const Patch* patch,
                   Patch::FaceType face,                 
                   CCVariable<Vector>& vel_CC,           
                   Lodi_vars* lv,
                   const Vector& dx);
                    
  void FaceTemp_LODI(const Patch* patch,
                    const Patch::FaceType face,
                    CCVariable<double>& temp_CC,
                    Lodi_vars* lv, 
                    const Vector& dx);
               
  void FacePress_LODI(const Patch* patch,
                      CCVariable<double>& press_CC,
                      StaticArray<CCVariable<double> >& rho_micro,
                      SimulationStateP& sharedState, 
                      Patch::FaceType face,
                      Lodi_vars_pressBC* lv);
                          
} // End namespace Uintah
#endif
