/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013-2015 The plumed team
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
#ifndef __PLUMED_multicolvar_MultiColvarFunction_h
#define __PLUMED_multicolvar_MultiColvarFunction_h

#include "tools/Matrix.h"
#include "MultiColvarBase.h"
#include "AtomValuePack.h"
#include "CatomPack.h"
#include "vesselbase/StoreDataVessel.h"

namespace PLMD {
namespace multicolvar {

class MultiColvarFunction : public MultiColvarBase {
private:
/// A tempory vector that is used for retrieving vectors
  std::vector<double> tvals;
/// This sets up the atom list
  void setupAtomLists();
protected:
/// Get the derivatives for the central atom with index ind
  CatomPack getCentralAtomPackFromInput( const unsigned& ind ) const ;
///
  void getVectorForTask( const unsigned& ind, const bool& normed, std::vector<double>& orient0 ) const ;
///
  MultiValue& getVectorDerivatives( const unsigned& ind, const bool& normed ) const ;
///
  void mergeVectorDerivatives( const unsigned& ival, const unsigned& start, const unsigned& end,
                               const unsigned& jatom, const std::vector<double>& der, 
                               MultiValue& myder, AtomValuePack& myatoms ) const ;
/// Build sets by taking one multicolvar from each base
  void buildSets();
/// Build colvars for atoms as if they were symmetry functions
  void buildSymmetryFunctionLists();
/// Build a colvar for each pair of atoms
  void buildAtomListWithPairs( const bool& allow_intra_group );
/// Get the icolv th base multicolvar 
  MultiColvarBase* getBaseMultiColvar( const unsigned& icolv ) const ;
/// Get the total number of tasks that this calculation is based on
  unsigned getFullNumberOfBaseTasks() const ;
/// Get the number of base multicolvars 
  unsigned getNumberOfBaseMultiColvars() const ;
public:
  explicit MultiColvarFunction(const ActionOptions&);
  static void registerKeywords( Keywords& keys );
  bool threadSafe() const { return false; }
};

inline
unsigned MultiColvarFunction::getFullNumberOfBaseTasks() const {
  return colvar_label.size(); 
}


inline
unsigned MultiColvarFunction::getNumberOfBaseMultiColvars() const {
  return mybasemulticolvars.size(); 
}

inline
MultiColvarBase* MultiColvarFunction::getBaseMultiColvar( const unsigned& icolv ) const {
  plumed_dbg_assert( icolv<mybasemulticolvars.size() );
  return mybasemulticolvars[icolv]; 
} 

inline
CatomPack MultiColvarFunction::getCentralAtomPackFromInput( const unsigned& ind ) const {
  plumed_dbg_assert( ind<colvar_label.size() ); unsigned mmc=colvar_label[ind];
  unsigned basen=0;
  for(unsigned i=0;i<mmc;++i) basen+=mybasemulticolvars[i]->getNumberOfAtoms();
  return mybasemulticolvars[mmc]->getCentralAtomPack( basen, convertToLocalIndex(ind,mmc) );
}

inline
void MultiColvarFunction::getVectorForTask( const unsigned& ind, const bool& normed, std::vector<double>& orient ) const {
  plumed_dbg_assert( ind<colvar_label.size() ); unsigned mmc=colvar_label[ind];
  plumed_dbg_assert( mybasedata[mmc]->storedValueIsActive( convertToLocalIndex(ind,mmc) ) );
  mybasedata[mmc]->retrieveValue( convertToLocalIndex(ind,mmc), normed, orient );
}

}
}
#endif
