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
#include "AdjacencyMatrixBase.h"
#include "multicolvar/AtomValuePack.h"
#include "core/ActionRegister.h"
#include "tools/SwitchingFunction.h"
#include "tools/Matrix.h"

//+PLUMEDOC MATRIX ALIGNED_MATRIX 
/*
Adjacency matrix in which two molecule are adjacent if they are within a certain cutoff and if they have the same orientation.

\par Examples

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace adjmat {

class ContactAlignedMatrix : public AdjacencyMatrixBase {
private:
/// Get the number of quantities
  unsigned ncomp;
/// switching function
  Matrix<SwitchingFunction> switchingFunction;
public:
/// Create manual
  static void registerKeywords( Keywords& keys );
/// Constructor
  explicit ContactAlignedMatrix(const ActionOptions&);
/// Create the ith, ith switching function
  void setupConnector( const unsigned& id, const unsigned& i, const unsigned& j, const std::string& desc );
/// This actually calculates the value of the contact function
  void calculateWeight( const unsigned& taskCode, multicolvar::AtomValuePack& myatoms ) const ;
/// This does nothing
  double compute( const unsigned& tindex, multicolvar::AtomValuePack& myatoms ) const ;
///
  /// Used to check for connections between atoms
  bool checkForConnection( const std::vector<double>& myvals ) const { return !(myvals[1]>epsilon); }
};

PLUMED_REGISTER_ACTION(ContactAlignedMatrix,"ALIGNED_MATRIX")

void ContactAlignedMatrix::registerKeywords( Keywords& keys ){
  AdjacencyMatrixBase::registerKeywords( keys );
  keys.add("atoms","MOLECULES","The list of molecules for which you would like to calculate the contact matrix.  The molecules involved must "
                               "have an orientation so your list will be a list of the labels of \\ref mcolv or \\ref multicolvarfunction "
                               "as PLUMED calculates the orientations of molecules within these operations.  Please note also that the majority "
                               "of \\ref mcolv and \\ref multicolvarfunction do not calculate a molecular orientation.");
  keys.add("numbered","SWITCH","This keyword is used if you want to employ an alternative to the continuous swiching function defined above. "
                               "The following provides information on the \\ref switchingfunction that are available. "
                               "When this keyword is present you no longer need the NN, MM, D_0 and R_0 keywords.");
}

ContactAlignedMatrix::ContactAlignedMatrix( const ActionOptions& ao ):
Action(ao),
AdjacencyMatrixBase(ao),
ncomp(getSizeOfInputVectors())
{
  if( getSizeOfInputVectors()<3 ) error("base multicolvars do not calculate an orientation");

  // Read in the atomic positions
  std::vector<AtomNumber> atoms; parseAtomList("MOLECULES",-1,atoms);
  plumed_assert( atoms.size()==0 );
  // Read in the switching function
  switchingFunction.resize( getNumberOfNodeTypes(), getNumberOfNodeTypes() );
  parseConnectionDescriptions("SWITCH",0);

  // Find the largest sf cutoff
  double sfmax=switchingFunction(0,0).get_dmax();
  for(unsigned i=0;i<getNumberOfNodeTypes();++i){
      for(unsigned j=0;j<getNumberOfNodeTypes();++j){
          double tsf=switchingFunction(i,j).get_dmax();
          if( tsf>sfmax ) sfmax=tsf;
      }
  }
  // And set the link cell cutoff
  setLinkCellCutoff( sfmax );

  // And request the atoms involved in this colvar
  std::vector<unsigned> dims(2); dims[0]=dims[1]=colvar_label.size();
  requestAtoms( atoms, true, false, dims );
}

void ContactAlignedMatrix::setupConnector( const unsigned& id, const unsigned& i, const unsigned& j, const std::string& desc ){
  plumed_assert( id==0 ); std::string errors; switchingFunction(j,i).set(desc,errors);
  if( errors.length()!=0 ) error("problem reading switching function description " + errors);
  if( j!=i) switchingFunction(i,j).set(desc,errors);
  log.printf("  %d th and %d th multicolvar groups must be aligned and must be within %s\n",i+1,j+1,(switchingFunction(i,j).description()).c_str() );
}

void ContactAlignedMatrix::calculateWeight( const unsigned& taskCode, multicolvar::AtomValuePack& myatoms ) const {
  Vector distance = getSeparation( myatoms.getPosition(0), myatoms.getPosition(1) );
  double dfunc, sw = switchingFunction( getBaseColvarNumber( myatoms.getIndex(0) ), getBaseColvarNumber( myatoms.getIndex(1) ) ).calculate( distance.modulo(), dfunc );
  myatoms.setValue(0,sw);
}

double ContactAlignedMatrix::compute( const unsigned& tindex, multicolvar::AtomValuePack& myatoms ) const {
  double f_dot, dot_df; 

  std::vector<double> orient0(ncomp), orient1(ncomp);
  getOrientationVector( myatoms.getIndex(0), true, orient0 );
  getOrientationVector( myatoms.getIndex(1), true, orient1 );
  double dot=0; for(unsigned k=2;k<orient0.size();++k) dot+=orient0[k]*orient1[k];
  f_dot=0.5*( 1 + dot ); dot_df=0.5; 

  // Retrieve the weight of the connection
  double weight = myatoms.getValue(0); myatoms.setValue(0,1.0); 

  if( !doNotCalculateDerivatives() ){
      Vector distance = getSeparation( myatoms.getPosition(0), myatoms.getPosition(1) );
      double dfunc, sw = switchingFunction( getBaseColvarNumber( myatoms.getIndex(0) ), getBaseColvarNumber( myatoms.getIndex(1) ) ).calculate( distance.modulo(), dfunc );
      addAtomDerivatives( 1, 0, (-dfunc)*f_dot*distance, myatoms );
      addAtomDerivatives( 1, 1, (+dfunc)*f_dot*distance, myatoms ); 
      myatoms.addBoxDerivatives( 1, (-dfunc)*f_dot*Tensor(distance,distance) ); 

      // Add derivatives of orientation 
      for(unsigned k=2;k<orient0.size();++k){ orient0[k]*=sw*dot_df; orient1[k]*=sw*dot_df; }
      addOrientationDerivatives( 1, 0, orient1, myatoms );
      addOrientationDerivatives( 1, 1, orient0, myatoms );
  }
  return weight*f_dot;
}

}
}

