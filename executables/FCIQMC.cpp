/*
  Developed by Sandeep Sharma 
  Copyright (c) 2017, Sandeep Sharma
  
  This file is part of DICE.
  
  This program is free software: you can redistribute it and/or modify it under the terms
  of the GNU General Public License as published by the Free Software Foundation, 
  either version 3 of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License along with this program. 
  If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#ifndef SERIAL
#include "mpi.h"
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi.hpp>
#endif

#include "Determinants.h"
#include "input.h"
#include "integral.h"
#include "SHCIshm.h"
#include "runFCIQMC.h"

#include "AGP.h"
#include "CorrelatedWavefunction.h"
#include "Jastrow.h"
#include "Slater.h"
#include "SelectedCI.h"
#include "trivialWF.h"
#include "trivialWalk.h"
#include "runVMC.h"

void printVMCHeader() {
  if (commrank == 0) {
    cout << endl << " Performing VMC..." << endl << endl;
  }
}

void printFCIQMCHeader() {
  if (commrank == 0) {
    cout << endl << " Performing FCIQMC..." << endl << endl;
  }
}

int main(int argc, char *argv[])
{
#ifndef SERIAL
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;
#endif
  startofCalc = getTime();

  initSHM();
  //license();
  if (commrank == 0) {
    std::system("echo User:; echo $USER");
    std::system("echo Hostname:; echo $HOSTNAME");
    std::system("echo CPU info:; lscpu | head -15");
    std::system("echo Computation started at:; date");
    cout << "git commit: " << GIT_HASH << ", branch: " << GIT_BRANCH << ", compiled at: " << COMPILE_TIME << endl << endl;
    cout << "Number of MPI processes used: " << commsize << endl << endl; 
  }

  string inputFile = "input.dat";
  if (argc > 1)
    inputFile = string(argv[1]);
  readInput(inputFile, schd, false);

  generator = std::mt19937(schd.seed + commrank);

  readIntegralsAndInitializeDeterminantStaticVariables("FCIDUMP");

  int norbs = Determinant::norbs;
  int nalpha = Determinant::nalpha;
  int nbeta = Determinant::nbeta;
  int nel = nalpha + nbeta;

  if (!schd.useTrialFCIQMC) {
    TrivialWF wave;
    TrivialWalk walk;
    runFCIQMC(wave, walk, norbs, nel, nalpha, nbeta);
  }
  else if (schd.wavefunctionType == "jastrowagp") {
    CorrelatedWavefunction<Jastrow, AGP> wave;
    Walker<Jastrow, AGP> walk;
    if (schd.restart || schd.fullRestart) {
      wave.readWave();
      wave.initWalker(walk);
    } else {
      printVMCHeader();
      runVMC(wave, walk);
    }
    printFCIQMCHeader();
    runFCIQMC(wave, walk, norbs, nel, nalpha, nbeta);
  }
  else if (schd.wavefunctionType == "jastrowslater") {
    CorrelatedWavefunction<Jastrow, Slater> wave;
    Walker<Jastrow, Slater> walk;
    if (schd.restart || schd.fullRestart) {
      wave.readWave();
      wave.initWalker(walk);
    } else {
      printVMCHeader();
      runVMC(wave, walk);
    }
    printFCIQMCHeader();
    runFCIQMC(wave, walk, norbs, nel, nalpha, nbeta);
  }
  else if (schd.wavefunctionType == "selectedci") {
    SelectedCI wave;
    SimpleWalker walk;
    wave.readWave();
    wave.initWalker(walk);
    runFCIQMC(wave, walk, norbs, nel, nalpha, nbeta);
  }

  boost::interprocess::shared_memory_object::remove(shciint2.c_str());
  boost::interprocess::shared_memory_object::remove(shciint2shm.c_str());
  return 0;
}
