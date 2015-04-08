/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2015 The plumed team
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

//+PLUMEDOC MCOLVAR FCCUBIC    
/*
*/
//+ENDPLUMEDOC


class Fccubic : public multicolvar::MultiColvar {
private:
//  double nl_cut;
  double rcut2, alpha, a1, b1;
  double phi, theta, psi;
  double rotationmatrix[3][3]; 
  std::vector<Vector> dlist; // A buffer array to store distances and apply PBC wholesale
  
  SwitchingFunction switchingFunction;
public:
  static void registerKeywords( Keywords& keys );
  Fccubic(const ActionOptions&);
// active methods:
  virtual double compute(); 
  Vector getCentralAtom();
/// Returns the number of coordinates of the field
  bool isPeriodic(){ return false; }
};

PLUMED_REGISTER_ACTION(Fccubic,"FCCUBIC")

void Fccubic::registerKeywords( Keywords& keys ){
  multicolvar::MultiColvar::registerKeywords( keys );
  keys.use("SPECIES"); keys.use("SPECIESA"); keys.use("SPECIESB");
  keys.add("compulsory","NN","6","The n parameter of the switching function ");
  keys.add("compulsory","MM","12","The m parameter of the switching function ");
  keys.add("compulsory","D_0","0.0","The d_0 parameter of the switching function");
  keys.add("compulsory","R_0","The r_0 parameter of the switching function");
  keys.add("optional","SWITCH","This keyword is used if you want to employ an alternative to the continuous swiching function defined above. "
                               "The following provides information on the \\ref switchingfunction that are available. "
                               "When this keyword is present you no longer need the NN, MM, D_0 and R_0 keywords.");
  keys.add("optional","ALPHA","The alpha parameter of the angular function");
  keys.add("optional","PHI","The Euler rotational angle phi");
  keys.add("optional","THETA","The Euler rotational angle theta");
  keys.add("optional","PSI","The Euler rotational angle psi");
  // Use actionWithDistributionKeywords
  keys.use("MEAN"); keys.use("MORE_THAN"); keys.use("LESS_THAN"); keys.use("MAX");
  keys.use("MIN"); keys.use("BETWEEN"); keys.use("HISTOGRAM"); keys.use("MOMENTS");
}

Fccubic::Fccubic(const ActionOptions&ao):
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
  
  std::string salpha; parse("ALPHA", salpha);
  if (salpha.length()>0) {
	  Tools::convert(salpha,alpha);
	  log.printf("  using ALPHA %f\n", alpha);
  } else { alpha=3.0; } // defaults to the Angioletti paper
  
  // Set the orientation for the fcc harmonic function
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
  setLinkCellCutoff( switchingFunction.get_dmax() );

  // Scaling factors such that '1' corresponds to fcc lattice
  // and '0' corresponds to isotropic (liquid)
  a1 = 80080. / (2717. + 16*alpha); b1 = 16.*(alpha-143)/(2717+16*alpha);

  // Read in the atoms
  int natoms=2; readAtoms( natoms );
  // And setup the ActionWithVessel
  checkRead();
}

double Fccubic::compute(){
   weightHasDerivatives=true;
   double value=0, norm=0, dfunc; Vector rotatedis;

   // Calculate the coordination number
   Vector myder, rotateder, fder; 
   double sw, t0, t1, t2, t3, x2, x4, y2, y4, z2, z4, r4, r8, r12, air12, xy4, xz4, yz4, xyz4, tmp;
   unsigned nat=getNAtoms();
   
   //std::valarray<Vector> dlist(getPosition(0),getNAtoms());
   if (getNAtoms()>dlist.size()) dlist.resize(getNAtoms());
   for(unsigned i=1;i<nat;++i) dlist[i]=getPosition(i)-getPosition(0);
   applyPbc( dlist, nat );
   
   double d2; 
   for(unsigned i=1;i<nat;++i){
      Vector& distance=dlist[i]; 
      
      if ( (d2=distance[0]*distance[0])<rcut2 && 
           (d2+=distance[1]*distance[1])<rcut2 &&
           (d2+=distance[2]*distance[2])<rcut2) {
         
         r4 = d2*d2;         
         r8 = r4*r4;
         r12 = r8*r4;
           
         sw = switchingFunction.calculateSqr( d2, dfunc ); 
   
         norm += sw;

         rotatedis[0]=rotationmatrix[0][0]*distance[0]
                  +rotationmatrix[0][1]*distance[1]
                  +rotationmatrix[0][2]*distance[2];
         rotatedis[1]=rotationmatrix[1][0]*distance[0]
                  +rotationmatrix[1][1]*distance[1]
                  +rotationmatrix[1][2]*distance[2];
         rotatedis[2]=rotationmatrix[2][0]*distance[0]
                  +rotationmatrix[2][1]*distance[1]
                  +rotationmatrix[2][2]*distance[2];
                  
         x2 = rotatedis[0]*rotatedis[0];
         x4 = x2*x2;

         y2 = rotatedis[1]*rotatedis[1];
         y4 = y2*y2;

         z2 = rotatedis[2]*rotatedis[2];
         z4 = z2*z2;      

         xy4 = x4*y4; xz4 = x4*z4; yz4 = y4*z4;
         xyz4 = xy4*z4;  
         air12 = alpha/r12;
         
         tmp = ((xy4)+(xz4)+(yz4))/r8-xyz4*air12;
         t3 = (2*tmp-xyz4*air12)/d2;         

         t0 = (x2*y4+x2*z4)/r8-x2*yz4*air12;
         t1 = (y2*x4+y2*z4)/r8-y2*xz4*air12;
         t2 = (z2*x4+z2*y4)/r8-z2*xy4*air12;
         
 
         rotateder[0]=4*rotatedis[0]*(t0-t3);
         rotateder[1]=4*rotatedis[1]*(t1-t3);
         rotateder[2]=4*rotatedis[2]*(t2-t3);
         
         myder[0]=rotationmatrix[0][0]*rotateder[0]
                  +rotationmatrix[1][0]*rotateder[1]
                  +rotationmatrix[2][0]*rotateder[2];
         myder[1]=rotationmatrix[0][1]*rotateder[0]
                  +rotationmatrix[1][1]*rotateder[1]
                  +rotationmatrix[2][1]*rotateder[2];
         myder[2]=rotationmatrix[0][2]*rotateder[0]
                  +rotationmatrix[1][2]*rotateder[1]
                  +rotationmatrix[2][2]*rotateder[2];
         
         tmp = a1*tmp+b1;
         myder *= a1;
          
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
   updateActiveAtoms(); 
   quotientRule( 0, 1, 0 ); 
   clearDerivativesAfterTask(1);
   // Weight doesn't really have derivatives (just use the holder for convenience)
   weightHasDerivatives=false; setElementValue( 1, 1.0 );

   return value / norm; // this is equivalent to getting an "atomic" CV
}

Vector Fccubic::getCentralAtom(){
   addCentralAtomDerivatives( 0, Tensor::identity() );
   return getPosition(0);
}

}
}

