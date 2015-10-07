/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2014 The plumed team
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
#include "multicolvar/MultiColvar.h"
#include "tools/NeighborList.h"
#include "core/ActionRegister.h"
#include "tools/SwitchingFunction.h"

#include <string>
#include <cmath>

using namespace std;

namespace PLMD{
namespace crystallization{

//+PLUMEDOC MCOLVAR BCCUBIC    
/*
*/
//+ENDPLUMEDOC


class Bccubic : public multicolvar::MultiColvar {
private:
//  double nl_cut;
  double rcut2;
  double phi, theta, psi;
  double rotationmatrix[3][3]; 
  SwitchingFunction switchingFunction;
public:
  static void registerKeywords( Keywords& keys );
  Bccubic(const ActionOptions&);
// active methods:
  virtual double compute(); 
  Vector getCentralAtom();
/// Returns the number of coordinates of the field
  bool isPeriodic(){ return false; }
};

PLUMED_REGISTER_ACTION(Bccubic,"BCCUBIC")

void Bccubic::registerKeywords( Keywords& keys ){
  multicolvar::MultiColvar::registerKeywords( keys );
  keys.use("SPECIES"); keys.use("SPECIESA"); keys.use("SPECIESB");
  keys.add("compulsory","NN","6","The n parameter of the switching function ");
  keys.add("compulsory","MM","12","The m parameter of the switching function ");
  keys.add("compulsory","D_0","0.0","The d_0 parameter of the switching function");
  keys.add("compulsory","R_0","The r_0 parameter of the switching function");
  keys.add("optional","SWITCH","This keyword is used if you want to employ an alternative to the continuous swiching function defined above. "
                               "The following provides information on the \\ref switchingfunction that are available. "
                               "When this keyword is present you no longer need the NN, MM, D_0 and R_0 keywords.");
  keys.add("optional","PHI","The Euler rotational angle phi");
  keys.add("optional","THETA","The Euler rotational angle theta");
  keys.add("optional","PSI","The Euler rotational angle psi");
  // Use actionWithDistributionKeywords
  keys.use("MEAN"); keys.use("MORE_THAN"); keys.use("LESS_THAN"); keys.use("MAX");
  keys.use("MIN"); keys.use("BETWEEN"); keys.use("HISTOGRAM"); keys.use("MOMENTS");
}

Bccubic::Bccubic(const ActionOptions&ao):
PLUMED_MULTICOLVAR_INIT(ao)
{
  // Read in the switching function
  std::string sw, errors; parse("SWITCH",sw);
  if(sw.length()>0){
     switchingFunction.set(sw,errors);
     if( errors.length()!=0 ) error("problem reading SWITCH keyword : " + errors );
  } else { 
     double r_0=-1.0, d_0; int nn, mm;
     parse("NN",nn); parse("MM",mm);
     parse("R_0",r_0); parse("D_0",d_0);
     if( r_0<0.0 ) error("you must set a value for R_0");
     switchingFunction.set(nn,mm,r_0,d_0);
  }
  
  // Set the orientation for the bcc harmonic function
  std::string sphi; parse("PHI", sphi);
  if (sphi.length()>0) {
	  Tools::convert(sphi,phi);
	  log.printf("  using PHI %f\n", phi);
  } else { phi=0.0; }
  
  std::string stheta; parse("THETA", stheta);
  if (stheta.length()>0) {
	  Tools::convert(stheta,theta);
	  log.printf("  using THETA %f\n", theta);
  } else { theta=0.0; }
  
  std::string spsi; parse("PSI", spsi);
  if (spsi.length()>0) {
	  Tools::convert(spsi,psi);
	  log.printf("  using PSI %f\n", psi);
  } else { psi=0.0; } 
  
  // Calculate the rotation matrix http://mathworld.wolfram.com/EulerAngles.html
  rotationmatrix[0][0]=cos(psi)*cos(phi)-cos(theta)*sin(phi)*sin(psi);
  rotationmatrix[0][1]=cos(psi)*sin(phi)+cos(theta)*cos(phi)*sin(psi);
  rotationmatrix[0][2]=sin(psi)*sin(theta);
  
  rotationmatrix[1][0]=-sin(psi)*cos(phi)-cos(theta)*sin(phi)*cos(psi);
  rotationmatrix[1][1]=-sin(psi)*sin(phi)+cos(theta)*cos(phi)*cos(psi);
  rotationmatrix[1][2]=cos(psi)*sin(theta);
  
  rotationmatrix[2][0]=sin(theta)*sin(phi);
  rotationmatrix[2][1]=-sin(theta)*cos(phi);
  rotationmatrix[2][2]=cos(theta);
  
  
  log.printf("  measure of simple cubicity around central atom.  Includes those atoms within %s\n",( switchingFunction.description() ).c_str() );
  // Set the link cell cutoff
  rcut2 = switchingFunction.get_dmax()*switchingFunction.get_dmax();
  setLinkCellCutoff( 2.*switchingFunction.get_dmax() );

  // Read in the atoms
  int natoms=2; readAtoms( natoms );
  // And setup the ActionWithVessel
  checkRead();
}

double Bccubic::compute(){
   weightHasDerivatives=true;
   double value=0, norm=0, dfunc; Vector distance; Vector rotatedis;

   // Calculate the coordination number
   Vector myder, rotateder, fder;
   double sw, t0, t1, t2, t3, x2, y2, z2, xyz, xyz2, r6, r8, tmp, a1, b1;
   for(unsigned i=1;i<getNAtoms();++i){
      distance=getSeparation( getPosition(0), getPosition(i) );
      
      rotatedis[0]=rotationmatrix[0][0]*distance[0]
                  +rotationmatrix[0][1]*distance[1]
                  +rotationmatrix[0][2]*distance[2];
      rotatedis[1]=rotationmatrix[1][0]*distance[0]
                  +rotationmatrix[1][1]*distance[1]
                  +rotationmatrix[1][2]*distance[2];
      rotatedis[2]=rotationmatrix[2][0]*distance[0]
                  +rotationmatrix[2][1]*distance[1]
                  +rotationmatrix[2][2]*distance[2];
      
      double d2 = distance.modulo2();
      if( d2<rcut2 ){ 
         sw = switchingFunction.calculateSqr( d2, dfunc ); 
   
         norm += sw;
         // bcc = (x^2 y^2 z^2)/ r^6
         x2 = rotatedis[0]*rotatedis[0];
         y2 = rotatedis[1]*rotatedis[1];
         z2 = rotatedis[2]*rotatedis[2];
         
         xyz = rotatedis[0]*rotatedis[1]*rotatedis[2];
         xyz2 = xyz*xyz;
                 
         r6 = pow( distance.modulo2(), 3 );
         r8 = pow( distance.modulo2(), 4 );

         tmp = xyz2/r6;       
 
         rotateder[0] = 2.*rotatedis[0]*y2*z2*(y2+z2-2.*x2)/r8;
         rotateder[1] = 2.*rotatedis[1]*x2*z2*(x2+z2-2.*y2)/r8;
         rotateder[2] = 2.*rotatedis[2]*x2*y2*(x2+y2-2.*z2)/r8;
         
         myder[0]=rotationmatrix[0][0]*rotateder[0]
                  +rotationmatrix[1][0]*rotateder[1]
                  +rotationmatrix[2][0]*rotateder[2];
         myder[1]=rotationmatrix[0][1]*rotateder[0]
                  +rotationmatrix[1][1]*rotateder[1]
                  +rotationmatrix[2][1]*rotateder[2];
         myder[2]=rotationmatrix[0][2]*rotateder[0]
                  +rotationmatrix[1][2]*rotateder[1]
                  +rotationmatrix[2][2]*rotateder[2];
         
         // Scaling so that '1' corresponds to bcc lattice
         // and '0' corresponds to liquid
         // Numerically evaluated
         a1 = 36.3462; b1 = -0.346154;
         tmp = a1*tmp+b1;
         myder[0] = a1*myder[0]; 
         myder[1] = a1*myder[1];
         myder[2] = a1*myder[2];
          
         value += sw*tmp;
  
         fder = (+dfunc)*tmp*distance + sw*myder;

         addAtomsDerivatives( 0, -fder );
         addAtomsDerivatives( i, +fder );
         addBoxDerivatives( Tensor(distance,-fder) );
         addAtomsDerivativeOfWeight( 0, (-dfunc)*distance );
         addAtomsDerivativeOfWeight( i, (+dfunc)*distance );
         addBoxDerivativesOfWeight( (-dfunc)*Tensor(distance,distance) );
      }
   }
   
   setElementValue(0, value); setElementValue(1, norm ); 
   // values -> der of... value [0], weight[1], x coord [2], y, z... [more magic]
   updateActiveAtoms(); quotientRule( 0, 1, 0 ); clearDerivativesAfterTask(1);
   // Weight doesn't really have derivatives (just use the holder for convenience)
   weightHasDerivatives=false; setElementValue( 1, 1.0 );

   return value / norm; // this is equivalent to getting an "atomic" CV
}

Vector Bccubic::getCentralAtom(){
   addCentralAtomDerivatives( 0, Tensor::identity() );
   return getPosition(0);
}

}
}

