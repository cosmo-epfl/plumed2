/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013,2014 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#ifndef __PLUMED_mapping_Mapping_h
#define __PLUMED_mapping_Mapping_h

#include "core/ActionAtomistic.h"
#include "core/ActionWithValue.h"
#include "core/ActionWithArguments.h"
#include "vesselbase/ActionWithVessel.h"
#include "reference/PointWiseMapping.h"
#include <vector>

namespace PLMD {

class PDB;

namespace mapping {

class Mapping :
  public ActionAtomistic,
  public ActionWithArguments,
  public ActionWithValue,
  public vesselbase::ActionWithVessel
  {
private:
//  The derivative wrt to the distance from the frame
  std::vector<double> dfframes;
/// This holds all the reference information
  PointWiseMapping* mymap;
/// The forces on each of the derivatives (used in apply)
  std::vector<double> forcesToApply;
/// Which frames were calculated
  std::vector<bool> frameWasCalculated;
/// Stores all the distances
  std::vector<double> distances;
protected:
/// The (transformed) distance from each frame
  std::vector<double> fframes;
/// Get the number of frames in the path
  unsigned getNumberOfReferencePoints() const ;
/// Calculate the value of the distance from the ith frame
  double calculateDistanceFunction( const unsigned& ifunc, const bool& squared );
/// Store the distance function
  void storeDistanceFunction( const unsigned& ifunc );
/// Get the value of the weight
  double getWeight() const ;
public:
  static void registerKeywords( Keywords& keys );
  Mapping(const ActionOptions&);
  ~Mapping();
/// Overload the virtual functions that appear in both ActionAtomistic and ActionWithArguments
  void turnOnDerivatives();
  void needsDerivatives();
  void calculateNumericalDerivatives( ActionWithValue* a=NULL );
  void lockRequests();
  void unlockRequests();
/// Distance from a point is never periodic
  bool isPeriodic(){ return false; }
/// Get the number of derivatives for this action
  unsigned getNumberOfDerivatives();  // N.B. This is replacing the virtual function in ActionWithValue
/// Get the value of lambda for paths and property maps 
  virtual double getLambda();
/// This does the transformation of the distance by whatever function is required
  virtual double transformHD( const double& dist, double& df )=0;
/// This doe sthe transformation of the low dimensional distance
  virtual double transformLD( const double& dist, double& df )=0;
/// Get the number of properties we are projecting onto
  unsigned getNumberOfProperties() const ;
/// Get the name of the ith property we are projecting
  std::string getPropertyName( const unsigned& iprop ) const ;
/// Get the index of a particular named property 
  unsigned getPropertyIndex( const std::string& name ) const ;
/// Get the name of the ith argument
  std::string getArgumentName( unsigned& iarg );
/// Get the value of the ith property for the current frame
  double getPropertyValue( const unsigned& iprop ) const ;
/// Return the current value of the high dimensional function
  double getCurrentHighDimFunctionValue( const unsigned& ider ) const ;
/// Perform chain rule for derivatives
  void mergeDerivatives( const unsigned& , const double& );
/// Stuff to do before we do the calculation
  void prepare();
/// Apply the forces 
  void apply();
/// Find the projection of the point closests to this one in the high dimensional space
  void findClosestPoint( std::vector<double>& pp ) const ;
/// Calculate the value of the stress at a given point and the derivatives.
  double calculateStress( const std::vector<double>& pp, std::vector<double>& der );
};

inline
unsigned Mapping::getNumberOfReferencePoints() const {
  return mymap->getNumberOfMappedPoints();   
}

inline
unsigned Mapping::getNumberOfDerivatives(){
  unsigned nat=getNumberOfAtoms();
  if(nat>0) return 3*nat + 9 + getNumberOfArguments();
  return getNumberOfArguments();
}

inline
void Mapping::lockRequests(){
  ActionWithArguments::lockRequests();
  ActionAtomistic::lockRequests();
}

inline
void Mapping::unlockRequests(){
  ActionWithArguments::unlockRequests();
  ActionAtomistic::unlockRequests();
}

inline
unsigned Mapping::getNumberOfProperties() const {
  return mymap->getNumberOfProperties();   
}

inline
std::string Mapping::getPropertyName( const unsigned& iprop ) const {
  return mymap->getPropertyName(iprop);  
}

inline
double Mapping::getPropertyValue( const unsigned& iprop ) const {
  plumed_dbg_assert( iprop<getNumberOfProperties() );
  return mymap->getPropertyValue( getCurrentTask(), iprop ); 
}

inline
double Mapping::getWeight() const {
  return mymap->getWeight( getCurrentTask() ); 
}

inline 
double Mapping::getCurrentHighDimFunctionValue( const unsigned& ider ) const {
  plumed_dbg_assert( ider<2 );
  return fframes[ider*getNumberOfReferencePoints() + getCurrentTask()];
}

inline
void Mapping::storeDistanceFunction( const unsigned& ifunc ){
  plumed_dbg_assert( ifunc<getNumberOfReferencePoints() );
  unsigned storef=getNumberOfReferencePoints()+ifunc;
  fframes[storef]=fframes[ifunc]; dfframes[storef]=dfframes[ifunc];
  mymap->copyFrameDerivatives( ifunc, storef );
}

}
}
#endif
