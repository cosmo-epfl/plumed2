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
#include "Mapping.h"
#include "vesselbase/Vessel.h"
#include "reference/MetricRegister.h"
#include "tools/PDB.h"
#include "tools/Matrix.h"
#include "core/PlumedMain.h"
#include "core/Atoms.h"

namespace PLMD {
namespace mapping {

void Mapping::registerKeywords( Keywords& keys ){
  Action::registerKeywords( keys ); 
  ActionWithValue::registerKeywords( keys );
  ActionWithArguments::registerKeywords( keys ); 
  ActionAtomistic::registerKeywords( keys ); 
  vesselbase::ActionWithVessel::registerKeywords( keys );
  keys.add("compulsory","REFERENCE","a pdb file containing the set of reference configurations");
  keys.add("compulsory","PROPERTY","the property to be used in the index. This should be in the REMARK of the reference");
  keys.add("compulsory","TYPE","OPTIMAL","the manner in which distances are calculated. More information on the different "
                                         "metrics that are available in PLUMED can be found in the section of the manual on "
                                         "\\ref dists");
  keys.addFlag("DISABLE_CHECKS",false,"disable checks on reference input structures.");
}

Mapping::Mapping(const ActionOptions&ao):
Action(ao),
ActionAtomistic(ao),
ActionWithArguments(ao),
ActionWithValue(ao),
ActionWithVessel(ao)
{
  // Read the input
  std::string mtype; parse("TYPE",mtype);
  bool skipchecks; parseFlag("DISABLE_CHECKS",skipchecks);
  // Setup the object that does the mapping
  mymap = new PointWiseMapping( mtype, skipchecks ); 
 
  // Read the properties we require
  if( keywords.exists("PROPERTY") ){
     std::vector<std::string> property;
     parseVector("PROPERTY",property);
     if(property.size()==0) error("no properties were specified");
     mymap->setPropertyNames( property, false );
  } else {
     std::vector<std::string> property(1); 
     property[0]="spath";
     mymap->setPropertyNames( property, true );
  }

  // Open reference file
  std::string reference; parse("REFERENCE",reference); 
  FILE* fp=fopen(reference.c_str(),"r"); 
  if(!fp) error("could not open reference file " + reference );

  // Read all reference configurations 
  bool do_read=true; std::vector<double> weights; 
  unsigned nfram=0, wnorm=0., ww;
  while (do_read){
     PDB mypdb; 
     // Read the pdb file
     do_read=mypdb.readFromFilepointer(fp,plumed.getAtoms().usingNaturalUnits(),0.1/atoms.getUnits().getLength());
     // Fix argument names
     expandArgKeywordInPDB( mypdb );
     if(do_read){
        mymap->readFrame( mypdb ); ww=mymap->getWeight( nfram ); 
        weights.push_back( ww );
        wnorm+=ww; nfram++;
     } else {
        break;
     }
  }
  fclose(fp); 

  if(nfram==0 ) error("no reference configurations were specified");
  log.printf("  found %u configurations in file %s\n",nfram,reference.c_str() );
  for(unsigned i=0;i<weights.size();++i) weights[i] /= wnorm;
  frameWasCalculated.resize( nfram ); mymap->setWeights( weights );
  distances.resize( nfram );

  // Finish the setup of the mapping object
  // Get the arguments and atoms that are required
  std::vector<AtomNumber> atoms; std::vector<std::string> args;
  mymap->getAtomAndArgumentRequirements( atoms, args );
  requestAtoms( atoms ); std::vector<Value*> req_args;
  interpretArgumentList( args, req_args ); requestArguments( req_args );
  // Duplicate all frames (duplicates are used by sketch-map)
  mymap->duplicateFrameList(); 
  fframes.resize( 2*nfram, 0.0 ); dfframes.resize( 2*nfram, 0.0 );
  plumed_assert( !mymap->mappingNeedsSetup() );
  // Resize all derivative arrays
  mymap->setNumberOfAtomsAndArguments( atoms.size(), args.size() );
  // Resize forces array
  if( getNumberOfAtoms()>0 ){ 
     forcesToApply.resize( 3*getNumberOfAtoms() + 9 + getNumberOfArguments() );
  } else {
     forcesToApply.resize( getNumberOfArguments() );
  }
}

void Mapping::turnOnDerivatives(){
  ActionWithValue::turnOnDerivatives();
  needsDerivatives();
} 

void Mapping::needsDerivatives(){
  ActionWithValue::turnOnDerivatives();
  ActionWithVessel::needsDerivatives(); 
}

Mapping::~Mapping(){
  delete mymap;
}

void Mapping::prepare(){
  if( mymap->mappingNeedsSetup() ){
      // Get the arguments and atoms that are required
      std::vector<AtomNumber> atoms; std::vector<std::string> args;
      mymap->getAtomAndArgumentRequirements( atoms, args );
      requestAtoms( atoms ); std::vector<Value*> req_args; 
      interpretArgumentList( args, req_args ); requestArguments( req_args );
      // Duplicate all frames (duplicates are used by sketch-map)
      mymap->duplicateFrameList();
      // Get the number of frames in the path
      unsigned nfram=getNumberOfReferencePoints();
      fframes.resize( 2*nfram, 0.0 ); dfframes.resize( 2*nfram, 0.0 ); 
      plumed_assert( !mymap->mappingNeedsSetup() );
      // Resize all derivative arrays
      mymap->setNumberOfAtomsAndArguments( atoms.size(), args.size() );
      // Resize forces array
      forcesToApply.resize( 3*getNumberOfAtoms() + 9 + getNumberOfArguments() );
  }
  // Keep track of the fact that none of the distances were calculated
  for(unsigned i=0;i<frameWasCalculated.size();++i) frameWasCalculated[i]=false;
}

unsigned Mapping::getPropertyIndex( const std::string& name ) const {
  return mymap->getPropertyIndex( name );
}

double Mapping::getLambda(){
  plumed_merror("lambda is not defined in this mapping type");
}

std::string Mapping::getArgumentName( unsigned& iarg ){
  if( iarg < getNumberOfArguments() ) return getPntrToArgument(iarg)->getName();
  unsigned iatom=iarg - getNumberOfArguments();
  std::string atnum; Tools::convert( getAbsoluteIndex(iatom).serial(),atnum);
  unsigned icomp=iatom%3;
  if(icomp==0) return "pos" + atnum + "x";
  if(icomp==1) return "pos" + atnum + "y";
  return "pos" + atnum + "z"; 
} 

double Mapping::calculateDistanceFunction( const unsigned& ifunc, const bool& squared ){
  // Note that frame was calculated
  frameWasCalculated[ifunc]=true;
  // Calculate the distance
  distances[ifunc] = mymap->calcDistanceFromConfiguration( ifunc, getPositions(), getPbc(), getArguments(), squared );     
  // Transform distance by whatever
  fframes[ifunc]=transformHD( distances[ifunc], dfframes[ifunc] );
  return fframes[ifunc];
}

void Mapping::calculateNumericalDerivatives( ActionWithValue* a ){
  if( getNumberOfArguments()>0 ){
     ActionWithArguments::calculateNumericalDerivatives( a );
  }
  if( getNumberOfAtoms()>0 ){
     Matrix<double> save_derivatives( getNumberOfComponents(), getNumberOfArguments() );
     for(unsigned j=0;j<getNumberOfComponents();++j){
        for(unsigned i=0;i<getNumberOfArguments();++i) save_derivatives(j,i)=getPntrToComponent(j)->getDerivative(i);
     }
     calculateAtomicNumericalDerivatives( a, getNumberOfArguments() ); 
     for(unsigned j=0;j<getNumberOfComponents();++j){ 
        for(unsigned i=0;i<getNumberOfArguments();++i) getPntrToComponent(j)->addDerivative( i, save_derivatives(j,i) );
     }
  }
}

void Mapping::mergeDerivatives( const unsigned& ider, const double& df ){
  unsigned cur = getCurrentTask(), frameno=ider*getNumberOfReferencePoints() + cur;
  for(unsigned i=0;i<getNumberOfArguments();++i){
      accumulateDerivative( i, df*dfframes[frameno]*mymap->getArgumentDerivative(frameno,i) );
  }
  if( getNumberOfAtoms()>0 ){
      Vector ader; Tensor tmpvir; tmpvir.zero();
      unsigned n=getNumberOfArguments(); 
      for(unsigned i=0;i<getNumberOfAtoms();++i){
          ader=mymap->getAtomDerivatives( frameno, i );            
          accumulateDerivative( n, df*dfframes[frameno]*ader[0] ); n++;
          accumulateDerivative( n, df*dfframes[frameno]*ader[1] ); n++;
          accumulateDerivative( n, df*dfframes[frameno]*ader[2] ); n++;
          tmpvir += -1.0*Tensor( getPosition(i), ader );
      }
      Tensor vir; 
      if( !mymap->getVirial( frameno, vir ) ) vir=tmpvir;
      accumulateDerivative( n, df*dfframes[frameno]*vir(0,0) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(0,1) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(0,2) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(1,0) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(1,1) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(1,2) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(2,0) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(2,1) ); n++;
      accumulateDerivative( n, df*dfframes[frameno]*vir(2,2) ); 
  }
}

void Mapping::apply(){
  if( getForcesFromVessels( forcesToApply ) ){
     addForcesOnArguments( forcesToApply );
     if( getNumberOfAtoms()>0 ) setForcesOnAtoms( forcesToApply, getNumberOfArguments() );
  }
}

void Mapping::findClosestPoint( std::vector<double>& pp ) const {
  plumed_dbg_assert( pp.size()==getNumberOfProperties() );
#ifndef NDEBUG
  for(unsigned i=0;i<getNumberOfReferencePoints();++i) plumed_dbg_assert( frameWasCalculated[i] );
#endif
  double mindist=distances[0]; unsigned refp=0;
  for(unsigned i=1;i<getNumberOfReferencePoints();++i){
     if( distances[i]<mindist ){ refp=i; mindist=distances[i]; }
  }
  for(unsigned i=0;i<pp.size();++i) pp[i] = mymap->getPropertyValue( refp, i );
}

double Mapping::calculateStress( const std::vector<double>& pp, std::vector<double>& der ){
  plumed_dbg_assert( pp.size()==getNumberOfProperties() && der.size()==getNumberOfProperties() );
#ifndef NDEBUG
  for(unsigned i=0;i<getNumberOfReferencePoints();++i) plumed_dbg_assert( frameWasCalculated[i] );
#endif
  std::vector<double> tmpder( getNumberOfProperties() );
  double df, chi2=0.0; der.assign(der.size(),0.0);
  for(unsigned i=0;i<getNumberOfReferencePoints();++i){
     double dist=0.0;
     for(unsigned j=0;j<getNumberOfProperties();++j){
         tmpder[j] = pp[j] - mymap->getPropertyValue( i, j );
         dist += tmpder[j]*tmpder[j];
     }
     dist=transformLD( sqrt(dist), df );
     double tmp = fframes[i] - dist; 
     // Accumulate the stress 
     chi2 += mymap->getWeight(i)*tmp*tmp;
     // Accumulate the derivatives
     for(unsigned j=0;j<getNumberOfProperties();++j) der[j] += 2*mymap->getWeight(i)*tmp*df*tmpder[j];
  }
  return chi2;
} 

}
}


