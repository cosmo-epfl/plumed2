/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

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
#include "SMACOF.h"
#include "reference/PointWiseMapping.h"

namespace PLMD {
namespace analysis {

void SMACOF::run( const Matrix<double>& Weights, PointWiseMapping* mymap, const double& tol ){
   // Get distances between trajectory frames
   Matrix<double> Distances( mymap->modifyDmat() ); unsigned M = Distances.nrows();

   // Retrieve old projection
   Matrix<double> InitialZ( M, mymap->getNumberOfProperties() );
   for(unsigned i=0; i<M; ++i){
      for(unsigned j=0; j<mymap->getNumberOfProperties(); ++j) InitialZ(i,j) =  mymap->getProjectionCoordinate( i, j );    
   }
   
   // Calculate V
   Matrix<double> V(M,M); double totalWeight=0.;
   for(unsigned i=0; i<M; ++i){
     for(unsigned j=0; j<M; ++j){
        if(i==j) continue;
        V(i,j)=-Weights(i,j);
        if( j<i ) totalWeight+=Weights(i,j);
     }
     for(unsigned j=0; j<M; ++j){
       if(i==j)continue;
       V(i,i)-=V(i,j);
     }   
   }    
    
    // And pseudo invert V
    Matrix<double> mypseudo(M, M);
    pseudoInvert(V, mypseudo);


    Matrix<double> dists( M, M ); double myfirstsig = calculateSigma( Weights, Distances, InitialZ, dists ) / totalWeight;    

    // initial sigma is made up of the original distances minus the distances between the projections all squared.
    unsigned MAXSTEPS=1000; Matrix<double> temp( M, M ), BZ( M, M ), newZ( M, mymap->getNumberOfProperties() );
    for(unsigned n=0;n<MAXSTEPS;++n){
        if(n==MAXSTEPS-1) plumed_merror("ran out of steps in SMACOF algorithm");

        // Recompute BZ matrix
        for(unsigned i=0; i<M; ++i){
           for(unsigned j=0; j<M; ++j){
              if(i==j) continue;  //skips over the diagonal elements

              if( dists(i,j)>0 ) BZ(i,j) = -Weights(i,j)*Distances(i,j) / dists(i,j); 
              else BZ(i,j)=0.;
           }
           //the diagonal elements are -off diagonal elements BZ(i,i)-=BZ(i,j)   (Equation 8.25)
           BZ(i,i)=0; //create the space memory for the diagonal elements which are scalars
           for(unsigned j=0; j<M; ++j){ 
              if(i==j) continue;
              BZ(i,i)-=BZ(i,j); 
           }
        }
        
        mult( mypseudo, BZ, temp); mult(temp, InitialZ, newZ);   
        //Compute new sigma
        double newsig = calculateSigma( Weights, Distances, newZ, dists ) / totalWeight;
        printf("SIGMA VALUES %d %f %f %f \n",n,myfirstsig,newsig,myfirstsig-newsig);
        //Computing whether the algorithm has converged (has the mass of the potato changed
        //when we put it back in the oven!)
        if( fabs( newsig - myfirstsig )<tol ) break;    
        myfirstsig=newsig;       
        InitialZ = newZ;
    } 

    // Pass final projections to map object
    for(unsigned i=0;i<M;++i){
        for(unsigned j=0;j<mymap->getNumberOfProperties();++j) mymap->setProjectionCoordinate( i, j, newZ(i,j) ); 
    }

}

double SMACOF::calculateSigma( const Matrix<double>& Weights, const Matrix<double>& Distances, const Matrix<double>& InitialZ, Matrix<double>& dists ){   
    unsigned M = Distances.nrows(); double sigma=0;  
    for(unsigned i=1; i<M; ++i){
       for(unsigned j=0; j<i; ++j){
          double dlow=0;  
          for(unsigned k=0; k<InitialZ.ncols();++k){
             double tmp2=InitialZ(i,k) - InitialZ(j,k); 
              dlow+=tmp2*tmp2;
          }
          dists(i,j)=dists(j,i)=sqrt(dlow);
          double tmp3= Distances(i,j) - dists(i,j);
          sigma+= Weights(i,j)*tmp3*tmp3;         
       }
    }
    return sigma;     
}      

}
}
