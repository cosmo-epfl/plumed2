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
#include "ReferenceAtoms.h"
#include "tools/OFile.h"
#include "tools/PDB.h"

namespace PLMD {

ReferenceAtoms::ReferenceAtoms( const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration(ro)
{
}

void ReferenceAtoms::readAtomsFromPDB( const PDB& pdb ){
  if( pdb.getNumberOfAtomBlocks()!=1 ) error("found multi-atom-block pdb format but expecting only one block of atoms");  

  for(unsigned i=0;i<pdb.size();++i){
     indices.push_back( pdb.getAtomNumbers()[i] ); reference_atoms.push_back( pdb.getPositions()[i] );
     align.push_back( pdb.getOccupancy()[i] ); displace.push_back( pdb.getBeta()[i] );
  }
  der_index.resize( reference_atoms.size() );
}

void ReferenceAtoms::setAtomNumbers( const std::vector<AtomNumber>& numbers ){
  reference_atoms.resize( numbers.size() ); align.resize( numbers.size() );
  displace.resize( numbers.size() ); der_index.resize( numbers.size() );
  indices.resize( numbers.size() );
  for(unsigned i=0;i<numbers.size();++i){
     indices[i]=numbers[i]; der_index[i]=i; 
  }
}

void ReferenceAtoms::printAtoms( const unsigned& lunits, OFile& ofile ) const {
  for(unsigned i=0;i<reference_atoms.size();++i){
      ofile.printf("ATOM  %4d  X    RES   %4u %8.3f %8.3f %8.3f %6.2f %6.2f\n",
        indices[i].serial(), i, 
        lunits*reference_atoms[i][0], lunits*reference_atoms[i][1], lunits*reference_atoms[i][2],
        align[i], displace[i] );
  }
}

bool ReferenceAtoms::parseAtomList( const std::string& key, std::vector<unsigned>& numbers ){
  plumed_assert( numbers.size()==0 );

  std::vector<std::string> strings; 
  if( !parseVector(key,strings,true) ) return false;
  Tools::interpretRanges(strings); 

  numbers.resize( strings.size() ); 
  bool found; AtomNumber atom;
  for(unsigned i=0;i<strings.size();++i){
      if( !Tools::convert(strings[i],atom ) ) error("could not convert " + strings[i] + " into atom number");

      found=false;
      for(unsigned j=0;j<indices.size();++j){
          if( atom==indices[j] ){ found=true; numbers[i]=j; break; }
      }
      if(!found) error("atom labelled " + strings[i] + " is not present in pdb input file");
  }
  return true;
}

void ReferenceAtoms::getAtomRequests( std::vector<AtomNumber>& numbers, bool disable_checks ){
  singleDomainRequests(numbers,disable_checks);
}

void ReferenceAtoms::singleDomainRequests( std::vector<AtomNumber>& numbers, bool disable_checks ){
  checks_were_disabled=disable_checks;
  der_index.resize( indices.size() );

  if( numbers.size()==0 ){
      for(unsigned i=0;i<indices.size();++i){
         numbers.push_back( indices[i] );
         der_index[i]=i;
      }
  } else {
      if(!disable_checks){
         if( numbers.size()!=indices.size() ) error("mismatched numbers of atoms in pdb frames");
      }

      bool found;
      for(unsigned i=0;i<indices.size();++i){
         found=false;
         if(!disable_checks){
            if( indices[i]!=numbers[i] ) error("found mismatched reference atoms in pdb frames");
            der_index[i]=i;
         } else {
            for(unsigned j=0;j<numbers.size();++j){
              if( indices[i]==numbers[j] ){ found=true; der_index[i]=j; break; }
            } 
            if( !found ){
              der_index[i]=numbers.size(); numbers.push_back( indices[i] );
            }
         }
      }
  }
}

}
