//----- Stream.h -----------------------------------------------

#ifndef Uintah_Components_Arches_Stream_h
#define Uintah_Components_Arches_Stream_h

/**************************************
CLASS
   Stream
   
   Class Stream creates and stores the mixing variables that are used in Arches

GENERAL INFORMATION
   Stream.h - declaration of the class
   
   Author: Rajesh Rawat (rawat@crsim.utah.edu)
   Revised by: Jennifer Spinti (spinti@crsim.utah.edu)
   
   Creation Date:   July 20, 2000
   Last Revised:   July 16, 2001
   
   C-SAFE 
   
   Copyright U of U 2000

KEYWORDS

DESCRIPTION

WARNING
   none

************************************************************************/
#include <vector>

namespace Uintah {
    class ChemkinInterface;
    // Low temperature limit; used in addStream
    const double TLIM = 200.0;
 
    class Stream {
    public:
      Stream();
      Stream(int numSpecies, int numElements);
      Stream(int numSpecies, int numElements,
	     int numMixVars, int numRxnVars, bool lsoot);
      ///////////////////////////////////////////////////////////////////////
      //
      // Copy Constructor
      //         
      //
      Stream(const Stream& strm); // copy constructor

      // GROUP: Operators:
      ///////////////////////////////////////////////////////////////////////
      //
      // Assignment Operator 
      //         
      //
      Stream& operator=(const Stream &rhs);
      ~Stream();
      Stream& linInterpolate(double upfactor, double lowfactor,
			     Stream& rightvalue);
      void addStream(const Stream& strm, ChemkinInterface* chemInterf,
		     const double factor) ;
      void addSpecies(const ChemkinInterface* chemInterf, 
		      const char* speciesName, double mfrac);
      int speciesIndex(const ChemkinInterface* chemInterf, const char* name);
      void print(std::ostream& out) const;
      void print(std::ostream& out, ChemkinInterface* chemInterf);
      //std::vector<double> convertStreamToVec(const bool lsoot);
      std::vector<double> convertStreamToVec();
      void convertVecToStream(const std::vector<double>& vec_stateSpace, 
			      bool flag, int numMixVars, 
                              int numRxnVars, bool lsoot);
      double getValue(int count, bool flag);
      void normalizeStream();
      inline double getDensity() const {
	return d_density;
      }
      inline int getDepStateSpaceVars() const {
	// 4 correspond to density, pressure, temp, enthalpy
	return d_depStateSpaceVars;
      }
      inline double getEnthalpy() const {
	return d_enthalpy;
      }
      inline double getTemperature() const {
	return d_temperature;
      }
      inline double getCO2() const {
	return d_speciesConcn[3];
      }
      inline double getSootFV() const {
	return d_sootData[1];
      }
      inline double getRxnSource() const {
	return d_rxnVarRates[0];
      }

    public:
      double d_pressure; // Pa
      double d_density; // kg/m^3
      double d_temperature; // K
      double d_enthalpy;     // J/Kg
      double d_sensibleEnthalpy; //J/Kg
      double d_moleWeight;
      double d_cp; // J/Kg
      bool d_mole;
      int d_depStateSpaceVars;
      std::vector<double> d_speciesConcn; // Mass or mole fraction in
      // constructor; converted to mass fraction in addStream
      std::vector<double> d_atomNumbers; // kg-atoms element I/kg mixture
      std::vector<double> d_rxnVarRates; // mass fraction/s
      std::vector<double> d_rxnVarNorm; // min/max values of rxn parameter
      std::vector<double> d_sootData; // soot volume fraction and average diameter
      int d_numMixVars;
      int d_numRxnVars;
      bool d_lsoot;

    private:
      // includes all the vars except vectors...
      // increase the value if want to increase number of variables
      //
      static const int NUM_DEP_VARS = 7;

    }; // End class Stream

}  // End namespace Uintah

#endif

