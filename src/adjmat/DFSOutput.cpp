/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2014,2015 The plumed team
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
#include "DFSBase.h"
#include "tools/OFile.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/ActionPilot.h"
#include "core/ActionRegister.h"

//+PLUMEDOC MATRIXF DFSOUTPUT
/*

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace adjmat {

class DFSOutput : public ActionPilot {
private:
  OFile ofile;
  DFSBase* myclusters;
  unsigned clustr;
public:
  static void registerKeywords( Keywords& keys );
  explicit DFSOutput(const ActionOptions&);
  void calculate(){}
  void apply(){}
  void update();
};

PLUMED_REGISTER_ACTION(DFSOutput,"DFSOUTPUT")

void DFSOutput::registerKeywords( Keywords& keys ){
  Action::registerKeywords( keys );
  ActionPilot::registerKeywords( keys );
  keys.add("compulsory","DFS","the DFS action that performed the clustering");
  keys.add("compulsory","CLUSTER","1","which cluster would you like to look at 1 is the largest cluster, 2 is the second largest, 3 is the the third largest and so on");
  keys.add("compulsory","STRIDE","1","the frequency with which you would like to output the atoms in the cluster");
  keys.add("compulsory","FILE","the name of the file on which to output the details of the cluster");
}

DFSOutput::DFSOutput(const ActionOptions& ao):
Action(ao),
ActionPilot(ao),
myclusters(NULL)
{
  // Setup output file
  ofile.link(*this); std::string file; parse("FILE",file);
  if( file.length()==0 ) error("output file name was not specified");
  ofile.open(file); log.printf("  on file %s \n",file.c_str());

  // Find what action we are taking the clusters from
  std::vector<std::string> matname(1); parse("DFS",matname[0]);
  myclusters = plumed.getActionSet().selectWithLabel<DFSBase*>( matname[0] );
  if( !myclusters ) error( matname[0] + " does not calculate perform a clustering of the atomic positions"); 
  addDependency( myclusters );

  // Read in the cluster we are calculating
  parse("CLUSTER",clustr);
  if( clustr<1 ) error("cannot look for a cluster larger than the largest cluster");
  if( clustr>myclusters->getNumberOfNodes() ) error("cluster selected is invalid - too few atoms in system");
  log.printf("  outputting atoms in %u th largest cluster found by %s \n",clustr,matname[0].c_str() );
}

void DFSOutput::update(){
  std::vector<unsigned> myatoms; myclusters->retrieveAtomsInCluster( clustr, myatoms );
  ofile.printf("CLUSTERING RESULTS AT TIME %f : NUMBER OF ATOMS IN %u TH LARGEST CLUSTER EQUALS %u \n",getTime(),clustr,static_cast<unsigned>(myatoms.size()) );
  ofile.printf("INDICES OF ATOMS : ");
  for(unsigned i=0;i<myatoms.size();++i) ofile.printf("%d ",(myclusters->getAbsoluteIndexOfCentralAtom(myatoms[i])).index());
  ofile.printf("\n");
}

}
}


