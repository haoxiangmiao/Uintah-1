//May 6, 2011 This version was checked out from SCIRun/UintahFresh/src/CCA/Components/Models/Radiation/RMCRT which
// was updated on Fri May 4 at 1ish pm.  I will now switch from cell iterator to i,j,k.  Eventually
// I will also switch from the flat arrays to 3D arrays.
//----- Ray.cc ----------------------------------------------
#include <CCA/Components/Models/Radiation/RMCRT/Ray.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Grid/DbgOutput.h>
#include <time.h>

//--------------------------------------------------------------
//
using namespace Uintah;
using namespace std;
static DebugStream dbg("RAY", false);
//---------------------------------------------------------------------------
// Method: Constructor. he's not creating an instance to the class yet
//---------------------------------------------------------------------------
Ray::Ray()
{
  _pi = acos(-1); 

  // sigma*T^4
  d_sigmaT4_label = VarLabel::create( "sigmaT4", CCVariable<double>::getTypeDescription() ); 
  divQ_label      = VarLabel::create( "divQ",    CCVariable<double>::getTypeDescription() ); 
  d_matlSet = 0;
}

//---------------------------------------------------------------------------
// Method: Destructor
//---------------------------------------------------------------------------
Ray::~Ray()
{
  VarLabel::destroy(d_sigmaT4_label);
  VarLabel::destroy(divQ_label);
  
  if(d_matlSet && d_matlSet->removeReference()) {
    delete d_matlSet;
  }
}

//---------------------------------------------------------------------------
// Method: Problem setup (access to input file information)
//---------------------------------------------------------------------------
void
Ray::problemSetup( const ProblemSpecP& inputdb) 
{
  ProblemSpecP db = inputdb;

  db->getWithDefault( "NoOfRays"  , _NoOfRays  , 1000 );
  db->getWithDefault( "Threshold" , _Threshold , 0.01 );      //When to terminate a ray
  db->getWithDefault( "Alpha"     , _alpha     , 0.2 );       //Absorption coefficient of the boundaries
  db->getWithDefault( "Slice"     , _slice     , 9 );         //Level in z direction of xy slice
  db->getWithDefault( "benchmark_1" , _benchmark_1, false );  //probably need to make this smarter...
                                                              //depending on what isaac has in mind
  db->getWithDefault("StefanBoltzmann", _sigma, 5.67051e-8);  // Units are W/(m^2-K)
  _sigma_over_pi = _sigma/_pi;

}

//______________________________________________________________________
// Register the material index and label names
void
Ray::registerVarLabels(int   matlIndex,
                       const VarLabel* abskg,
                       const VarLabel* absorp,
                       const VarLabel* temperature )
{
  d_matl             = matlIndex;
  d_abskgLabel       = abskg;
  d_absorpLabel      = absorp;
  d_temperatureLabel = temperature;
  
  //__________________________________
  //  define the materialSet
  d_matlSet = scinew MaterialSet();
  vector<int> m;
  m.push_back(matlIndex);
  d_matlSet->addAll(m);
  d_matlSet->addReference();
}
//---------------------------------------------------------------------------
//
void 
Ray::sched_initProperties( const LevelP& level, SchedulerP& sched, const int time_sub_step )
{

  std::string taskname = "Ray::schedule_initProperties"; 
  Task* tsk = scinew Task( taskname, this, &Ray::initProperties, time_sub_step ); 
  printSchedule(level,dbg,taskname);
  
  if ( time_sub_step == 0 ) { 
    tsk->requires( Task::OldDW, d_abskgLabel,       Ghost::None, 0 ); 
    tsk->requires( Task::OldDW, d_temperatureLabel, Ghost::None, 0 ); 
    tsk->computes( d_sigmaT4_label ); 
    tsk->computes( d_abskgLabel ); 
    tsk->computes( d_absorpLabel );

  } else { 
    tsk->requires( Task::NewDW, d_temperatureLabel, Ghost::None, 0 ); 
    tsk->modifies( d_sigmaT4_label ); 
    tsk->modifies( d_abskgLabel ); 
    tsk->modifies( d_absorpLabel ); 
  }

  sched->addTask( tsk, level->eachPatch(), d_matlSet ); 

}
//______________________________________________________________________
//
void
Ray::initProperties( const ProcessorGroup* pc,
                     const PatchSubset* patches,
                     const MaterialSubset* matls,
                     DataWarehouse* old_dw,
                     DataWarehouse* new_dw, 
                     const int time_sub_step )
{
  // patch loop
  const Level* level = getLevel(patches);
  
  for (int p=0; p < patches->size(); p++){

    const Patch* patch = patches->get(p);
    printTask(patches,patch,dbg,"Doing Ray::InitProperties");
    
    CCVariable<double> abskg; 
    CCVariable<double> absorp; 
    CCVariable<double> sigmaT4;

    constCCVariable<double> temperature; 

    if ( time_sub_step == 0 ) { 

      new_dw->allocateAndPut( abskg,    d_abskgLabel,     d_matl, patch ); 
      new_dw->allocateAndPut( sigmaT4,  d_sigmaT4_label,  d_matl, patch ); 
      new_dw->allocateAndPut( absorp,   d_absorpLabel,    d_matl, patch ); 

      abskg.initialize  ( 0.0 ); 
      absorp.initialize ( 0.0 ); 
      sigmaT4.initialize( 0.0 );

      old_dw->get(temperature,      d_temperatureLabel, d_matl, patch, Ghost::None, 0);

    } else { 

      new_dw->getModifiable( sigmaT4, d_sigmaT4_label,  d_matl, patch ); 
      new_dw->getModifiable( absorp,  d_absorpLabel,    d_matl, patch ); 
      new_dw->getModifiable( abskg,   d_abskgLabel,     d_matl, patch ); 
      new_dw->get( temperature,     d_temperatureLabel, d_matl, patch, Ghost::None, 0 ); 

    }
    
    IntVector pLow;
    IntVector pHigh;
    level->findInteriorCellIndexRange(pLow, pHigh);

    int Nx = pHigh[0] - pLow[0];
    int Ny = pHigh[1] - pLow[1];
    int Nz = pHigh[2] - pLow[2];   

    Vector Dx = patch->dCell(); 

    for ( CellIterator iter = patch->getCellIterator(); !iter.done(); iter++ ){ 

      IntVector c = *iter; 

      if ( _benchmark_1 ) { 
        abskg[c] = 0.90 * ( 1.0 - 2.0 * fabs( ( c[0] - (Nx - 1.0) /2.0) * Dx[0]) )
                        * ( 1.0 - 2.0 * fabs( ( c[1] - (Ny - 1.0) /2.0) * Dx[1]) )
                        * ( 1.0 - 2.0 * fabs( ( c[2] - (Nz - 1.0) /2.0) * Dx[2]) ) 
                        + 0.1;

      } else { 

        // need to put radcal calulation here: 
        abskg[c] = 0.0; 
        absorp[c] = 0.0; 

      } 
      double temp2 = temperature[c] * temperature[c] ;
      sigmaT4[c] = _sigma_over_pi * temp2 * temp2; // \sigma T^4

    }
  }
}


//---------------------------------------------------------------------------
// 
//---------------------------------------------------------------------------
  void
Ray::sched_sigmaT4( const LevelP& level, 
                    SchedulerP& sched )
{

  std::string taskname = "Ray::sched_sigmaT4";
  Task* tsk= scinew Task( taskname, this, &Ray::sigmaT4 );

  printSchedule(level,dbg,taskname);
  
  tsk->requires( Task::OldDW, d_temperatureLabel, Ghost::None, 0 ); 
  tsk->computes(d_sigmaT4_label); 

  sched->addTask( tsk, level->eachPatch(), d_matlSet );
}
//---------------------------------------------------------------------------
// Compute total intensity over all wave lengths (sigma * Temperature^4)
//---------------------------------------------------------------------------
void
Ray::sigmaT4( const ProcessorGroup*,
              const PatchSubset* patches,           
              const MaterialSubset*,                
              DataWarehouse* old_dw,                
              DataWarehouse* new_dw )               
{

  for (int p=0; p < patches->size(); p++){

    const Patch* patch = patches->get(p);
    printTask(patches,patch,dbg,"Doing Ray::sigmaT4");
   
    double sigma_over_pi = _sigma/M_PI;
    
    constCCVariable<double> temp;
    constCCVariable<double> abskg;
    CCVariable<double> sigmaT4;             // sigma T ^4/pi
    
    old_dw->get(temp,               d_temperatureLabel,   d_matl, patch, Ghost::None, 0);  
    new_dw->allocateAndPut(sigmaT4, d_sigmaT4_label,      d_matl, patch);

    for (CellIterator iter = patch->getCellIterator();!iter.done();iter++){
      const IntVector& c = *iter;
      double T_sqrd = temp[c] * temp[c];
      sigmaT4[c] = sigma_over_pi * T_sqrd * T_sqrd;
    }
  }
}

//---------------------------------------------------------------------------
// Method: Schedule the ray tracer
//---------------------------------------------------------------------------
void
Ray::sched_rayTrace( const LevelP& level, SchedulerP& sched, const int time_sub_step )
{
  std::string taskname = "Ray::sched_rayTrace";
  Task* tsk= scinew Task( taskname, this, &Ray::rayTrace, time_sub_step );
  printSchedule(level,dbg,taskname);

  tsk->requires( Task::NewDW , d_abskgLabel  ,   Ghost::None , 0 );
  tsk->requires( Task::NewDW , d_sigmaT4_label , Ghost::None , 0 );
//  tsk->requires( Task::OldDW , d_lab->d_cellTypeLabel , Ghost::None , 0 );

  if( time_sub_step == 0 ){
    tsk->computes( divQ_label ); 
  } else {
    tsk->modifies( divQ_label );
  }
  sched->addTask( tsk, level->eachPatch(), d_matlSet );

}

//---------------------------------------------------------------------------
// Method: The actual work of the ray tracer
//---------------------------------------------------------------------------
void
Ray::rayTrace( const ProcessorGroup* pc,
               const PatchSubset* patches,
               const MaterialSubset* matls,
               DataWarehouse* old_dw,
               DataWarehouse* new_dw,
               const int time_sub_step )
{

  double start=clock();

  // patch loop
  for (int p=0; p < patches->size(); p++){

    const Patch* patch = patches->get(p);
    printTask(patches,patch,dbg,"Doing Ray::rayTrace");
    
    Vector ray_location;
    Vector ray_location_prev;

    // since we are only requiring temperature, we get it as a const.  !!May need to remove the const part...
    //because *ix_ptr will be changed.
    constCCVariable<double> sigmaT4;
    constCCVariable<double> abskg;
    CCVariable<double> divQ;

    double disMin;

    // CCVariable<double> *array_ptr = temperature; // this is the pointer to the start of the 3D "CCVariable" array
    // CCVariable<double> *ix_ptr = array_ptr; // this pointer is free to move along the members of the array

    //  getting the temperature from the DW
    new_dw->get( abskg          , d_abskgLabel ,   d_matl , patch , Ghost::None , 0);
    new_dw->get( sigmaT4        , d_sigmaT4_label, d_matl , patch , Ghost::None , 0);
    
    if( time_sub_step == 0 ){
      new_dw->allocateAndPut( divQ, divQ_label, d_matl, patch );
      divQ.initialize( 0.0 );
    }else{
      old_dw->getModifiable( divQ,  divQ_label, d_matl, patch );
    }
    IntVector pLow  = patch->getCellLowIndex();
    IntVector pHigh = patch->getCellHighIndex(); 
    int ii;//used as an index when creating 1D arrays out of 3d arrays

    int Nx = pHigh[0] - pLow[0];
    int Ny = pHigh[1] - pLow[1];
    int Nz = pHigh[2] - pLow[2];   
    int ix =(Nx)*(Ny)*(Nz);


    double absorb_coef[ix];                            // represents the fraction absorbed per cell length through a control volume
                                                       // the data warehouse values begin at zero
    double dx_absorb_coef[ix];                         // Dx multiplied by the absorption coefficient
    double Iout_cv[ix];                                // Array because each cell's intensity will be referenced so many...
                                                       // times through all the paths of other cell's rays
    double chi_Iin_cv;                                 // Iin multiplied by its respective chi.
    double Inet_cv[ix];                                // separate Inet necessary for surfaces. !! I don't think I need an array for this
    double chi;                                        // the absorption coefficient multiplied of the origin cell
    double fs;                                         // fraction remaining after all current reflections
    unsigned int size = 0;                             // current size of PathIndex !!move to Ray.h
    double rho = 1.0 - _alpha;                         // reflectivity
    double optical_thickness;                          // The running total of alpha*length !!move to Ray.h
    double optical_thickness_prev;                     // The running total of alpha*length !!move to Ray.h
    Vector Dx = patch->dCell();                        // cell spacing

    //double abskg1D[ix];                              // !! used to visualize abskg only.  otherwise comment out
    //int netFaceFlux[ix][6]; // net radiative flux on each face of each cell //0:_bottom, 1:_top; 2:_south, 3:_north; 4:_west, 5:_east

    const double* sigmaT4_ptr = const_cast<double*>( sigmaT4.getPointer() );
    const double* abskg_ptr = const_cast<double*>( abskg.getPointer() );


/*`==========TESTING==========*/
if( ( Dx.x() != Dx.y() ) || ( Dx.x() != Dx.z() ) || ( Dx.y() != Dx.z() ) ) {
  throw InternalError("rayTrace:: only works if dx == dy == dz", __FILE__, __LINE__);
} 
/*===========TESTING==========`*/


    //double absorb_coef_3D[Nz][Ny][Nx];

    //Make Iout a 1D while referencing temperature which is a 3D array. This pre-computes Iout since it gets referenced so much
    //Pointer way
    ii=0;    
    for ( k=0; k<Nz; k++){//the indeces of the cells
      for ( j=0; j<Ny; j++){
        for ( i=0; i<Nx; i++){
         
          Iout_cv[ii] = *sigmaT4_ptr; 
          absorb_coef[ii] = *abskg_ptr;//make 1D from 3D
          //for benchmark case, temperature is 64.804 everywhere
          //Iout_cv[ii] = 64.804361 * 64.804361 * 64.804361 * 64.804361 * _sigma_over_pi;//T^4*sigma/pi
          // absorb_coef[ii] = 0.9*(1-2*fabs((i -(Nx-1)/2.0)*Dx[0]))*(1-2*fabs((j -(Ny-1)/2.0)*Dx[1]))*(1-2*fabs((k -(Nz-1)/2.0)*Dx[2])) +0.1;//benchmark 99
          //next line is for visualization only.
          dx_absorb_coef[ii] = Dx.x()*absorb_coef[ii];//Used in optical thickness calculation.!!Adjust if cells are noncubic
                            // ^^^^  FIX ME
          
          sigmaT4_ptr++;
          abskg_ptr++;
          ii++;
        }
      }
    }

    ix = 0;  //This now represents the current cell in 1D(akin to c, not cur)

    //__________________________________
    //
    for (CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){ 

      IntVector origin = *iter; 
      int i = origin.x();
      int j = origin.y();
      int k = origin.z();

      chi_Iin_cv = 0; 
      chi = absorb_coef[ix];
      
      // ray loop
      for (int iRay=0; iRay < _NoOfRays; iRay++){

        IntVector cur = origin;

        //  Each ray will always begin at origin
        int cx = ix;
        int cx_p;
        // begins at 0. Can follow a ray without touching ix.(akin to cur)

        _mTwister.seed((i + j +k) * iRay +1); 

        ray_location[0] =   i +  _mTwister.rand() ;
        ray_location[1] =   j +  _mTwister.rand() ;
        ray_location[2] =   k +  _mTwister.rand() ;

        // see http://www.cgafaq.info/wiki/aandom_Points_On_Sphere for explanation

        double plusMinus_one = 2 * _mTwister.rand() - 1;
        double r = sqrt(1 - plusMinus_one * plusMinus_one);    // Radius of circle at z
        double theta = 2 * M_PI * _mTwister.rand();            // Uniform betwen 0-2Pi

        Vector direction_vector;
        direction_vector[0] = r*cos(theta);                   // Convert to cartesian
        direction_vector[1] = r*sin(theta);
        direction_vector[2] = plusMinus_one;                  // Uniform between -1 to 1

        Vector inv_direction_vector = Vector(1.0)/direction_vector;

        int step[3];                                          // Gives +1 or -1 based on sign
        bool sign[3];                 // **** Why is this a bool?  --Todd

        //bool opposite_sign[3];
        for ( ii= 0; ii<3; ii++){
          if (inv_direction_vector[ii]>0){
            step[ii] = 1;
            sign[ii] = 1;
            //  opposite_sign[ii]=0;
          }
          else{
            step[ii] = -1;
            sign[ii] = 0;// 
            //opposite_sign[ii]=1;
          }
        }

        double tMaxX = (i + sign[0] - ray_location[0]) * inv_direction_vector[0];
        double tMaxY = (j + sign[1] - ray_location[1]) * inv_direction_vector[1];
        double tMaxZ = (k + sign[2] - ray_location[2]) * inv_direction_vector[2];

        //Length of t to traverse one cell
        double tDeltaX = abs(inv_direction_vector[0]);
        double tDeltaY = abs(inv_direction_vector[1]);
        double tDeltaZ = abs(inv_direction_vector[2]);
        double tMax_prev = 0;
        bool in_domain = true;

        //Initializes the following values for each ray
        double intensity = 1.0;     
        optical_thickness = 0;
        fs = 1;
        
        //+++++++Begin ray tracing+++++++++++++++++++

        Vector temp_direction = direction_vector;   // **** Is this used anywhere?
        
        //save the direction vector so that it can get modified by...
        //the 2nd switch statement for reflections, but so that we can get the ray_location back into...
        //the domain after it was updated following the first switch statement.         

        //Threshold while loop
        while (intensity > _Threshold){

          //Domain while loop 
          while (in_domain){

            size++;
            cx_p =cx;

            //__________________________________
            //  Determine which cell the ray will enter next
            if (tMaxX < tMaxY){
              if (tMaxX < tMaxZ){
                cx        = cx + step[0];
                cur[0]    = cur[0] + step[0];
                disMin    = tMaxX - tMax_prev;
                tMax_prev = tMaxX;
                tMaxX     = tMaxX + tDeltaX;
              }
              else {
                cx        = cx + Nx*Ny*step[2];
                cur[2]    = cur[2] + step[2];
                disMin    = tMaxZ - tMax_prev;
                tMax_prev = tMaxZ;
                tMaxZ     = tMaxZ + tDeltaZ;
              }
            }
            else {
              if(tMaxY <tMaxZ){
                cx        = cx + Nx*step[1];
                cur[1]    = cur[1] + step[1];
                disMin    = tMaxY - tMax_prev;
                tMax_prev = tMaxY;
                tMaxY     = tMaxY + tDeltaY;
              }
              else {
                cx        = cx + Nx*Ny*step[2];
                cur[2]    = cur[2] + step[2];
                disMin    = tMaxZ - tMax_prev;
                tMax_prev = tMaxZ;
                tMaxZ     = tMaxZ + tDeltaZ;
              }
            }
            
            // Is this cell in the domain
            if( (cur[0] > Nx-1 || cur[0] < 0) ||
                (cur[1] > Ny-1 || cur[1] < 0) ||
                (cur[2] > Nz-1 || cur[2] < 0) ){
              in_domain = false; 
            }

            //__________________________________
            //  Update the ray location
            //this is necessary to find the absorb_coef at the endpoints of each step
            ray_location_prev = ray_location;                                  // ****** Is this used anywhere??
            ray_location      = ray_location + (disMin * direction_vector);

            //Because I do these next three lines before the switch statement, I will never have...
            //to worry about cells outside the boundary, or having to decrement opticalthickness or...
            //intensity after I get back inside the domain.
            optical_thickness_prev = optical_thickness;
            optical_thickness += dx_absorb_coef[cx_p]*disMin;

            intensity = intensity*exp(-optical_thickness);  //update intensity by Beer's Law

            size++;

            //Eqn 3-15, while accounting for fs. 
            //Third term inside the parentheses is accounted for in Inet. Chi is accounted for in Inet calc.
            chi_Iin_cv += chi * (Iout_cv[cx_p] * ( exp(-optical_thickness_prev) - exp(-optical_thickness) ) * fs );
            // Multiply Iin by the current chi, for each ray not just at the end of all the rays.

          } //end domain while loop.  ++++++++++++++

          //__________________________________
          //  Reflections
          if (intensity > _Threshold){

            //puts ray back inside the domain...; 
            intensity*=rho;
            //comment out for cold wall:  Iin_cv += _alpha * Iout_cv[cx] * exp(-optical_thickness)*fs;//!! Right now the temperature of the...
            //boundary is simply the temp of the cell just inside the wall.This is accounting for emission from the walls reacing the origin
            //for non-cold wall, make this a chi_Iin_cv.
            //Comment out for cold, black walls: fs*=rho;//update fs after above Iin reassignment because the reflection is not attenuated by itself.
          }  //end reflection if statement

        }  // end threshold while loop (ends ray tracing for that ray
      }  // Ray loop

      //__________________________________
      //  Compute divQ & Inet.  
      //  Iout is blackbody and must be multiplied by absorb_coef. absorb_coef is the kappa of 9.53.
      Inet_cv[ix] = Iout_cv[ix] * absorb_coef[ix] - (chi_Iin_cv/_NoOfRays); //the last term is from Paula's eqn 3.10
      
      divQ[origin] = Inet_cv[ix] * 4.0 * _pi; 

      ix++;   // update flat array index
      
    }  // end cell iterator 

    double end =clock();
    double efficiency = size/((end-start)/ CLOCKS_PER_SEC);

    cout<< endl;
    cout << " RMCRT REPORT: " << endl;
    cout << " Used "<< (end-start) * 1000 / CLOCKS_PER_SEC<< " milliseconds of CPU time. \n" << endl;// Convert time to ms 
    cout << " Size: " << size << endl;
    cout << " Efficiency: " << efficiency << " steps per sec" << endl;
    cout << endl; 

  }//end patch loop
} // end ray trace method



//______________________________________________________________________
// ISAAC's NOTES: 
//May 6. Changed to cell iterator
//Created Jan 31. Cleaned up comments, removed hard coding of T and abskg 
// Jan 19// I changed cx to be lagging.  This changed nothing in the RMS error, but may be important...
//when referencing a non-uniform temperature.
//Created Jan13. //  Ray_PW_const.cc Making this piecewise constant by using CC values. not interpolating
//Removed symmetry test. 
//Has a new equation for absorb_coef for chi and optical thickness calculations...
//I did this based on my findings in my intepolator
//Just commented out a few unnecessary vars
//No more hitch!  Fixed cx to not be incremented the first march, and...
//fixed the formula for absorb_coef and chi which reference ray_location
//Now can do a DelDotqline in each of the three coordinate directions, through the center
//Ray Visualization works, and is correct
//To plot out the rays in matlab
//Now we use an average of two values for a more precise value of absorb_coef rather...
//than using the cell centered absorb_coef
//Using the exact absorb_coef for chi by using formula.Beautiful results...
//see chi_is_exact_absorb_coef.eps in runcases folder
//FIXED THE VARIANCE REDUCTION PROBLEM BY GETTING NEW CHI FOR EACH RAY goes with Chi Fixed folder
//BENCHMARK CASE 99. 
//with error msg if slice is too big
//Based on Ray_bak_Oct15.cc which was Created Oct 13.
// Using Woo (and Amanatides) method//and it works!
//efficiency of approx 20Msteps/sec
//I try to wait to declare each variable until I need it
//Incorporates Steve's sperical way of generating a direction vector
//Back to ijk from cell iterator
//Now absorb_coef is hard coded in because abskg in DW is simply zero
//Now gets abskg from Dw
// with capability to print temperature profile to a file
//Now gets T from DW.  accounts for intensity coming back from surfaces. calculates
// the net Intensity for each cell. Still needs to send rays out from surfaces.Chi is inside while 
//loop. I took out the double domain while loop simply for readability.  I should put it back in 
//when running cases. if(sign[xyorz]) else. See Ray_bak_Aug10.cc for correct implementation. ix 
//is just (NxNyNz) rather than (xNxxNyxNz).  absorbing media. reflections, and simplified while 
//(w/precompute) loop. ray_location now represents what was formally called emiss_point.  It is by
// cell index, not by physical location.

