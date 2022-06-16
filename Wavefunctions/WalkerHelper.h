/*
  Developed by Sandeep Sharma with contributions from James E. T. Smith and Adam A. Holmes, 2017
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
#ifndef WalkerHelper_HEADER_H
#define WalkerHelper_HEADER_H

#include <unordered_set>
#include "Determinants.h"
#include "igl/slice.h"
#include "igl/slice_into.h"
#include "ShermanMorrisonWoodbury.h"
#include "Slater.h"
#include "MultiSlater.h"
#include "AGP.h"
#include "Pfaffian.h"
#include "CPS.h"
#include "Jastrow.h"
#include "SJastrow.h"
#include "Gutzwiller.h"
#include "RBM.h"
#include "JRBM.h"

template<typename Reference>
class WalkerHelper {
};

template<>
class WalkerHelper<Slater>
{

 public:
  HartreeFock hftype;                           //hftype same as that in slater
  std::array<MatrixXcd, 2> thetaInv;          //inverse of the theta matrix
  std::vector<std::array<complex<double>, 2>> thetaDet;    //determinant of the theta matrix, vector for multidet
  std::array<vector<int>, 2> openOrbs;       //set of open orbitals in the walker
  std::array<vector<int>, 2> closedOrbs;     //set of closed orbitals in the walker
  std::array<vector<int>, 2> closedOrbsRef;  //set of closed orbitals in the reference (zeroth det)
  std::vector<std::array<MatrixXcd, 2>> rTable;    //table used for efficiently, vector for multidet

  WalkerHelper() {};
  
  WalkerHelper(const Slater &w, const Determinant &d) 
  {
    hftype = w.hftype;
 
    //fill the spin strings for the walker and the zeroth reference det
    fillOpenClosedOrbs(d);
    closedOrbsRef[0].clear();
    closedOrbsRef[1].clear();
    w.getDeterminants()[0].getClosedAlphaBeta(closedOrbsRef[0], closedOrbsRef[1]);
  
    rTable.resize(w.getNumOfDets());
    thetaDet.resize(w.getNumOfDets());
  
    if (hftype == Generalized) {
      initInvDetsTablesGhf(w);
    }
    else {
      initInvDetsTables(w);
    }
  }

  void fillOpenClosedOrbs(const Determinant &d)
  {
    openOrbs[0].clear();
    openOrbs[1].clear();
    closedOrbs[0].clear();
    closedOrbs[1].clear();
    d.getOpenClosedAlphaBeta(openOrbs[0], closedOrbs[0], openOrbs[1], closedOrbs[1]);
  }

  void makeTable(const Slater &w, const MatrixXcd& inv, const Eigen::Map<VectorXi>& colClosed, int detIndex, bool sz)
  {
    Map<VectorXi> rowOpen(&openOrbs[sz][0], openOrbs[sz].size());
    rTable[detIndex][sz] = MatrixXcd::Zero(openOrbs[sz].size(), closedOrbs[sz].size()); 
    MatrixXcd HfopenTheta;
    igl::slice(w.getHforbs(sz), rowOpen, colClosed, HfopenTheta);
    rTable[detIndex][sz] = HfopenTheta * inv;
  }

  //void calcOtherDetsTables(const Slater& w, bool sz)
  //{
  //  Eigen::Map<VectorXi> rowClosed(&closedOrbs[sz][0], closedOrbs[sz].size());
  //  vector<int> cre(closedOrbs[sz].size(), -1), des(closedOrbs[sz].size(), -1);
  //  for (int x = 1; x < w.getNumOfDets(); x++) {
  //    MatrixXcd invCurrent;
  //    vector<int> ref;
  //    w.getDeterminants()[x].getClosed(sz, ref);
  //    Eigen::Map<VectorXi> colClosed(&ref[0], ref.size());
  //    getDifferenceInOccupation(w.getDeterminants()[x], w.getDeterminants()[0], cre, des, sz);
  //    double parity = w.getDeterminants()[0].parity(cre, des, sz);
  //    calculateInverseDeterminantWithColumnChange(thetaInv[sz], thetaDet[0][sz], invCurrent, thetaDet[x][sz], cre, des, rowClosed, closedOrbsRef[sz], w.getHforbs(sz));
  //    thetaDet[x][sz] *= parity;
  //    makeTable(w, invCurrent, colClosed, x, sz);
  //  }
  //}

  //commenting out calcotherdetstables, uncomment for multidet
  void initInvDetsTables(const Slater &w)
  {
    int norbs = Determinant::norbs;
    for (int sz = 0; sz < 2; sz++) {
      Eigen::Map<VectorXi> rowClosed(&closedOrbs[sz][0], closedOrbs[sz].size());
      Eigen::Map<VectorXi> colClosed(&closedOrbsRef[sz][0], closedOrbsRef[sz].size());
      MatrixXcd theta;
      igl::slice(w.getHforbs(sz), rowClosed, colClosed, theta); 
      Eigen::FullPivLU<MatrixXcd> lua(theta);
      if (lua.isInvertible()) {
        thetaInv[sz] = lua.inverse();
        thetaDet[0][sz] = lua.determinant();
      }
      else {
        cout << sz << " overlap with determinant not invertible" << endl;
        exit(0);
      }
      rTable[0][sz] = MatrixXcd::Zero(norbs, closedOrbs[sz].size()); 
      rTable[0][sz] = w.getHforbs(sz).block(0, 0, norbs, closedOrbs[sz].size()) * thetaInv[sz];
      
      //makeTable(w, thetaInv[sz], colClosed, 0, sz);
      //calcOtherDetsTables(w, sz);
    }
  }

  void concatenateGhf(const vector<int>& v1, const vector<int>& v2, vector<int>& result) const
  {
    int norbs = Determinant::norbs;
    result.clear();
    result = v1;
    result.insert(result.end(), v2.begin(), v2.end());    
    for (int j = v1.size(); j < v1.size() + v2.size(); j++)
      result[j] += norbs;
  }

  void makeTableGhf(const Slater &w, const Eigen::Map<VectorXi>& colTheta)
  {
    rTable[0][0] = MatrixXcd::Zero(openOrbs[0].size() + openOrbs[1].size(), closedOrbs[0].size() + closedOrbs[1].size()); 
    MatrixXcd ghfOpen;
    vector<int> rowVec;
    concatenateGhf(openOrbs[0], openOrbs[1], rowVec);
    Eigen::Map<VectorXi> rowOpen(&rowVec[0], rowVec.size());
    igl::slice(w.getHforbs(), rowOpen, colTheta, ghfOpen);
    rTable[0][0] = ghfOpen * thetaInv[0];
    rTable[0][1] = rTable[0][0];
  }

  void initInvDetsTablesGhf(const Slater &w)
  {
    int norbs = Determinant::norbs;
    vector<int> workingVec0, workingVec1;
    concatenateGhf(closedOrbs[0], closedOrbs[1], workingVec0);
    Eigen::Map<VectorXi> rowTheta(&workingVec0[0], workingVec0.size());
    concatenateGhf(closedOrbsRef[0], closedOrbsRef[1], workingVec1);
    Eigen::Map<VectorXi> colTheta(&workingVec1[0], workingVec1.size());
  
    MatrixXcd theta;
    igl::slice(w.getHforbs(), rowTheta, colTheta, theta); 
    Eigen::FullPivLU<MatrixXcd> lua(theta);
    if (lua.isInvertible()) {
      thetaInv[0] = lua.inverse();
      thetaDet[0][0] = lua.determinant();
    }
    else {
      Eigen::Map<VectorXi> v1(&closedOrbs[0][0], closedOrbs[0].size());
      Eigen::Map<VectorXi> v2(&closedOrbs[1][0], closedOrbs[1].size());
      cout << "alphaClosed\n" << v1 << endl << endl;
      cout << "betaClosed\n" << v2 << endl << endl;
      cout << "col\n" << colTheta << endl << endl;
      cout << theta << endl << endl;
      cout << "overlap with theta determinant not invertible" << endl;
      exit(0);
    }
    thetaDet[0][1] = 1.;
    rTable[0][0] = MatrixXcd::Zero(2*norbs, closedOrbs[0].size() + closedOrbs[1].size()); 
    rTable[0][0] = w.getHforbs().block(0, 0, 2*norbs, closedOrbs[0].size() + closedOrbs[1].size()) * thetaInv[0];
    rTable[0][1] = rTable[0][0];
    //makeTableGhf(w, colTheta);
  }

  //commenting out calcotherdetstables, uncomment for multidet
  void excitationUpdate(const Slater &w, vector<int>& cre, vector<int>& des, bool sz, double parity, const Determinant& excitedDet)
  {
    MatrixXcd invOld = thetaInv[sz];
    std::complex<double> detOld = thetaDet[0][sz];
    MatrixXcd tableOld = rTable[0][sz];
    Eigen::Map<Eigen::VectorXi> colClosed(&closedOrbsRef[sz][0], closedOrbsRef[sz].size());
    calculateInverseDeterminantWithRowChange(invOld, detOld, tableOld, thetaInv[sz], thetaDet[0][sz], rTable[0][sz], cre, des, colClosed, closedOrbs[sz], w.getHforbs(sz), 1);
    thetaDet[0][sz] *= parity;
    fillOpenClosedOrbs(excitedDet);
    //makeTable(w, thetaInv[sz], colClosed, 0, sz);
    //calcOtherDetsTables(w, sz);
  }

  void excitationUpdateGhf(const Slater &w, vector<int>& cre, vector<int>& des, bool sz, double parity, const Determinant& excitedDet)
  {
    vector<int> colVec;
    concatenateGhf(closedOrbsRef[0], closedOrbsRef[1], colVec);
    Eigen::Map<VectorXi> colTheta(&colVec[0], colVec.size());
    vector<int> rowIn;
    concatenateGhf(closedOrbs[0], closedOrbs[1], rowIn);
    MatrixXcd invOld = thetaInv[0];
    std::complex<double> detOld = thetaDet[0][0];
    MatrixXcd tableOld = rTable[0][0];
    calculateInverseDeterminantWithRowChange(invOld, detOld, tableOld, thetaInv[0], thetaDet[0][0], rTable[0][0], cre, des, colTheta, rowIn, w.getHforbs(), 1);
    rTable[0][1] = rTable[0][0];
    thetaDet[0][0] *= parity;
    fillOpenClosedOrbs(excitedDet);
    //makeTableGhf(w, colTheta);
  }

  void getRelIndices(int i, int &relI, int a, int &relA, bool sz) const 
  {
    //relI = std::lower_bound(closedOrbs[sz].begin(), closedOrbs[sz].end(), i) - closedOrbs[sz].begin();
    //relA = std::lower_bound(openOrbs[sz].begin(), openOrbs[sz].end(), a) - openOrbs[sz].begin();
    int factor = 0;
    if (hftype == 2 && sz != 0) factor = 1;
    relI = std::search_n(closedOrbs[sz].begin(), closedOrbs[sz].end(), 1, i) - closedOrbs[sz].begin() + factor * closedOrbs[0].size();
    //relA = std::search_n(openOrbs[sz].begin(), openOrbs[sz].end(), 1, a) - openOrbs[sz].begin() + factor * openOrbs[0].size();
    relA = a + factor * Determinant::norbs;
  }

};

template<>
class WalkerHelper<MultiSlater>
{

 public:
  MatrixXcd t;                    // A^{-1}
  complex<double> refOverlap;     // < n | phi_0 >
  std::vector<double> ciOverlaps; // Re (< n | phi_i >), include parity
  double totalOverlap;            // Re (< n | psi >)
  std::vector<complex<double>> ciOverlapRatios; // < n | phi_i > / < n | phi_0 >, include parity, for orb gradient
  complex<double> totalComplexOverlap;  // < n | psi >, for orb gradient
  std::vector<int> closedOrbs, openOrbs;    // set of closed and open orbitals in the walker
  MatrixXcd r, c, b, rt, tc, rtc_b;  // intermediate tables
  vector<Matrix4cd> tcSlice;      // intermediate tables

  WalkerHelper() {};
  
  WalkerHelper(const MultiSlater &w, const Determinant &d)
  {
    //fill the closed orbs for the walker
    closedOrbs.clear();
    openOrbs.clear();
    vector<int> closedBeta, openBeta;
    d.getOpenClosedAlphaBeta(openOrbs, closedOrbs, openBeta, closedBeta);
    for (int& c_i : closedBeta) c_i += Determinant::norbs;
    for (int& o_i : openBeta) o_i += Determinant::norbs;
    closedOrbs.insert(closedOrbs.end(), closedBeta.begin(), closedBeta.end());
    openOrbs.insert(openOrbs.end(), openBeta.begin(), openBeta.end());
    
    initInvDetsTables(w);
    if (commrank == 0) {
      if (refOverlap.real() == 0 || totalOverlap == 0) {
        cout << "refOverlap = " << refOverlap << ", totalOverlap = " << totalOverlap << endl;
        cout << "walker det: " << d << endl;
        cout << "ciOverlaps\n";
        for (double ci: ciOverlaps) cout << ci << endl;
      }
    }
  }

  void initInvDetsTables(const MultiSlater &w)
  {
    int norbs = Determinant::norbs;
  
    // inverse and refDet
    MatrixXcd a;
    Eigen::Map<VectorXi> occRows(&closedOrbs[0], closedOrbs.size());
    auto refCopy = w.ref;
    Eigen::Map<VectorXi> occColumns(&refCopy[0], refCopy.size());
    igl::slice(w.getHforbs(), occRows, occColumns, a); 
    Eigen::FullPivLU<MatrixXcd> lua(a);
    if (lua.isInvertible()) {
      t = lua.inverse();
      refOverlap = lua.determinant();
    }
    else {
      if (commrank == 0) {
        cout << "overlap with zeroth determinant not invertible" << endl;
        cout << "occRows\n" << occRows.transpose() <<  endl;
        cout << "occColumns\n" << occColumns.transpose() <<  endl;
        cout << "a\n" << a << endl;
        cout << "w.Hforbs\n" <<  w.Hforbs << endl;
      }
#ifndef SERIAL
  MPI_Barrier(MPI_COMM_WORLD);
#endif
      exit(0);
    }
    
    // tables
    // TODO: change table structure so that only unoccupied orbitals are present in r and c, 
    // this is not done currently because table updates are easier with all orbitals, but leads to bigger tables
    //VectorXi all = VectorXi::LinSpaced(2*norbs, 0, 2*norbs - 1);
    auto openCopy = w.open;
    Eigen::Map<VectorXi> openColumns(&openCopy[0], openCopy.size());
    Eigen::Map<VectorXi> openRows(&openOrbs[0], openOrbs.size());
    //igl::slice(w.getHforbs(), all, occColumns, r);
    //igl::slice(w.getHforbs(), occRows, all, c);
    igl::slice(w.getHforbs(), openRows, occColumns, r);
    igl::slice(w.getHforbs(), occRows, openColumns, c);
    igl::slice(w.getHforbs(), openRows, openColumns, b);
    rt = r * t;
    tc = t * c;
    rtc_b = rt * c - b;
    //rtc_b = rt * c - w.getHforbs();

    // overlaps with phi_i
    ciOverlaps.clear();
    ciOverlapRatios.clear();
    ciOverlaps.push_back(refOverlap.real());
    ciOverlapRatios.push_back(1.);
    totalOverlap = w.ciCoeffs[0] * ciOverlaps[0];
    totalComplexOverlap = w.ciCoeffs[0] * refOverlap;
    for (int i = 1; i < w.numDets; i++) {
      if (w.ciExcitations[i][0].size() == 4) {
        Matrix4cd sliceMat;
        igl::slice(tc, w.ciExcitations[i][0], w.ciExcitations[i][1], sliceMat);
        tcSlice.push_back(sliceMat);
        complex<double> ratio = sliceMat.determinant() * complex<double>(w.ciParity[i]);
        ciOverlapRatios.push_back(ratio);
        ciOverlaps.push_back((ratio * refOverlap).real());
        totalOverlap += w.ciCoeffs[i] * ciOverlaps[i];
        totalComplexOverlap += w.ciCoeffs[i] * ratio * refOverlap;
      }
      else {
        MatrixXcd sliceMat;
        igl::slice(tc, w.ciExcitations[i][0], w.ciExcitations[i][1], sliceMat);
        complex<double> ratio = calcDet(sliceMat) * complex<double>(w.ciParity[i]);
        ciOverlapRatios.push_back(ratio);
        ciOverlaps.push_back((ratio * refOverlap).real());
        totalOverlap += w.ciCoeffs[i] * ciOverlaps[i];
        totalComplexOverlap += w.ciCoeffs[i] * ratio * refOverlap;
      }
    }
  }

  void excitationUpdate(const MultiSlater &w, vector<int>& cre, vector<int>& des, bool sz, double parity, const Determinant& excitedDet)
  {
    // sherman morrison to update inverse
    // right now only table rt is updated efficiently
    auto refCopy = w.ref;
    Eigen::Map<VectorXi> occColumns(&refCopy[0], refCopy.size());
    MatrixXcd tOld = t;
    complex<double> overlapOld = refOverlap;
    MatrixXcd rtOld = rt;
    calculateInverseDeterminantWithRowChange(tOld, overlapOld, rtOld, t, refOverlap, rt, cre, des, occColumns, closedOrbs, w.getHforbs(), 0);
    refOverlap *= parity;
   
    int norbs = Determinant::norbs;
    closedOrbs.clear();
    openOrbs.clear();
    vector<int> closedBeta, openBeta;
    excitedDet.getOpenClosedAlphaBeta(openOrbs, closedOrbs, openBeta, closedBeta);
    for (int& c_i : closedBeta) c_i += norbs;
    for (int& o_i : openBeta) o_i += norbs;
    closedOrbs.insert(closedOrbs.end(), closedBeta.begin(), closedBeta.end());
    openOrbs.insert(openOrbs.end(), openBeta.begin(), openBeta.end());
    
    // TODO: these tables also need efficient updates
    auto openCopy = w.open;
    Eigen::Map<VectorXi> openColumns(&openCopy[0], openCopy.size());
    Eigen::Map<VectorXi> occRows(&closedOrbs[0], closedOrbs.size());
    Eigen::Map<VectorXi> openRows(&openOrbs[0], openOrbs.size());
    //VectorXi all = VectorXi::LinSpaced(2*norbs, 0, 2*norbs - 1);
    igl::slice(w.getHforbs(), openRows, occColumns, r);
    igl::slice(w.getHforbs(), occRows, openColumns, c);
    igl::slice(w.getHforbs(), openRows, openColumns, b);
    rt = r * t;
    tc = t * c;
    rtc_b = rt * c - b;
    //rtc_b = rt * c - w.getHforbs();
    
    // overlaps with phi_i
    ciOverlaps.clear();
    ciOverlapRatios.clear();
    ciOverlaps.push_back(refOverlap.real());
    ciOverlapRatios.push_back(1.);
    totalOverlap = w.ciCoeffs[0] * ciOverlaps[0];
    totalComplexOverlap = w.ciCoeffs[0] * refOverlap;
    size_t count4 = 0;
    for (int j = 1; j < w.numDets; j++) {
      int rank = w.ciExcitations[j][0].size();
      complex<double> sliceDet;
      if (rank == 1) sliceDet = tc(w.ciExcitations[j][0][0], w.ciExcitations[j][1][0]);
      
      else if (rank == 2) sliceDet = tc(w.ciExcitations[j][0][0], w.ciExcitations[j][1][0]) * tc(w.ciExcitations[j][0][1], w.ciExcitations[j][1][1])
- tc(w.ciExcitations[j][0][0], w.ciExcitations[j][1][1]) * tc(w.ciExcitations[j][0][1], w.ciExcitations[j][1][0]);
      
      else if (rank == 3) {
        //igl::slice(tc, w.ciExcitations[j][0], w.ciExcitations[j][1], sliceMat);
        Matrix3cd tcSlice;
        for  (int mu = 0; mu < 3; mu++) 
          for (int nu = 0; nu < 3; nu++) 
            tcSlice(mu, nu) = tc(w.ciExcitations[j][0][mu], w.ciExcitations[j][1][nu]);
        //sliceDet = calcDet(sliceMat);
        sliceDet = tcSlice.determinant();
      }
      
      else if (rank == 4) {
        //igl::slice(tc, w.ciExcitations[j][0], w.ciExcitations[j][1], sliceMat);
        for  (int mu = 0; mu < 4; mu++) 
          for (int nu = 0; nu < 4; nu++) 
            tcSlice[count4](mu, nu) = tc(w.ciExcitations[j][0][mu], w.ciExcitations[j][1][nu]);
        //sliceDet = calcDet(sliceMat);
        sliceDet = tcSlice[count4].determinant();
        count4++;
      }
      
      else {
        MatrixXcd sliceMat = MatrixXcd::Zero(rank, rank);
        //igl::slice(tc, w.ciExcitations[j][0], w.ciExcitations[j][1], sliceMat);
        for (int mu = 0; mu < rank; mu++) 
          for (int nu = 0; nu < rank; nu++) 
            sliceMat(mu, nu) = tc(w.ciExcitations[j][0][mu], w.ciExcitations[j][1][nu]);
        sliceDet = sliceMat.determinant();
      }
      
      ciOverlaps.push_back((sliceDet * refOverlap).real() * w.ciParity[j]);
      ciOverlapRatios.push_back(sliceDet * complex<double>(w.ciParity[j]));
      totalOverlap += w.ciCoeffs[j] * ciOverlaps[j];
      totalComplexOverlap += w.ciCoeffs[j] * ciOverlapRatios[j] * refOverlap;
    }
    
    if (commrank == 0) {
      if (refOverlap.real() == 0 || totalOverlap == 0) {
        cout << "refOverlap = " << refOverlap << ", totalOverlap = " << totalOverlap << endl;
        cout << "walker det: " << excitedDet << endl;
        cout << "ciOverlaps\n";
        for (double ci: ciOverlaps) cout << ci << endl;
      }
    }
  }

  void getRelIndices(int i, int &relI, int a, int &relA, bool sz) const
  {
    //relI = std::lower_bound(closedOrbs[sz].begin(), closedOrbs[sz].end(), i) - closedOrbs[sz].begin();
    relI = std::search_n(closedOrbs.begin(), closedOrbs.end(), 1, i + sz * Determinant::norbs) - closedOrbs.begin();
    relA = std::search_n(openOrbs.begin(), openOrbs.end(), 1, a + sz * Determinant::norbs) - openOrbs.begin();
    //relA = a + sz * Determinant::norbs;
  }

};

//template<>
//class WalkerHelper<BFSlater>
//{
//
// public:
//  HartreeFock hftype;                           //hftype same as that in slater
//  std::array<MatrixXcd, 2> theta;
//  std::array<MatrixXcd, 2> thetaInv;          //inverse of the theta matrix
//  std::array<complex<double>, 2> thetaDet;    //determinant of the theta matrix
//  std::array<vector<int>, 2> openOrbs;       //set of open orbitals in the walker
//  std::array<vector<int>, 2> closedOrbs;     //set of closed orbitals in the walker
//  std::vector<int> doublons, holons;         //doubly occupied and empty spatial orbs 
//  std::array<vector<int>, 2> closedOrbsRef;  //set of closed orbitals in the reference
//  std::array<MatrixXcd, 2> rTable;    //table used for efficiently
//
//  WalkerHelper() {};
//  
//  WalkerHelper(const BFSlater &w, const Determinant &d) 
//  {
//    hftype = w.hftype;
// 
//    fillOccupancyHelpers(d);
//    closedOrbsRef[0].clear();
//    closedOrbsRef[1].clear();
//    w.det.getClosedAlphaBeta(closedOrbsRef[0], closedOrbsRef[1]);
//
//    if (hftype == Generalized) {
//      initInvDetsTablesGhf(w);
//    }
//    else {
//      initInvDetsTables(w);
//    }
//  }
//
//  void fillOccupancyHelpers(const Determinant &d)
//  {
//    //fill the spin strings for the walker
//    openOrbs[0].clear();
//    openOrbs[1].clear();
//    closedOrbs[0].clear();
//    closedOrbs[1].clear();
//    d.getOpenClosedAlphaBeta(openOrbs[0], closedOrbs[0], openOrbs[1], closedOrbs[1]);
//   
//    //fill holons and doublons
//    doublons.clear();
//    holons.clear();
//    set_intersection(closedOrbs[0].begin(), closedOrbs[0].end(), closedOrbs[1].begin(), closedOrbs[1].end(), back_inserter(doublons));
//    set_intersection(openOrbs[0].begin(), openOrbs[0].end(), openOrbs[1].begin(), openOrbs[1].end(), back_inserter(holons));
//  }
//
//  void initInvDetsTables(const BFSlater &w)
//  {
//    int norbs = Determinant::norbs;
//    for (int sz = 0; sz < 2; sz++) {
//      Eigen::Map<VectorXi> rowClosed(&closedOrbs[sz][0], closedOrbs[sz].size());
//      Eigen::Map<VectorXi> colClosed(&closedOrbsRef[sz][0], closedOrbsRef[sz].size());
//      MatrixXcd hforbs = w.getHforbs(sz);
//      igl::slice(hforbs, rowClosed, colClosed, theta[sz]);
//      for (int i = 0; i < doublons.size(); i++) {
//        int relIndex = std::search_n(closedOrbs[sz].begin(), closedOrbs[sz].end(), 1, doublons[i]) - closedOrbs[sz].begin();
//        for (int j = 0; j < holons.size(); j++) {
//          theta[sz].row(relIndex) += w.bf(i, j) * hforbs.row(holons(j));
//        }
//      }
//
//      Eigen::FullPivLU<MatrixXcd> lua(theta[sz]);
//      if (lua.isInvertible()) {
//        thetaInv[sz] = lua.inverse();
//        thetaDet[sz] = lua.determinant();
//      }
//      else {
//        cout << sz << " overlap with determinant not invertible" << endl;
//        exit(0);
//      }
//
//      rTable[sz] = MatrixXcd::Zero(norbs, closedOrbs[sz].size()); 
//      rTable[sz] = hforbs.block(0, 0, norbs, closedOrbs[sz].size()) * thetaInv[sz];
//    }
//  }
//
//  void concatenateGhf(const vector<int>& v1, const vector<int>& v2, vector<int>& result) const
//  {
//    int norbs = Determinant::norbs;
//    result.clear();
//    result = v1;
//    result.insert(result.end(), v2.begin(), v2.end());    
//    for (int j = v1.size(); j < v1.size() + v2.size(); j++)
//      result[j] += norbs;
//  }
//
//  void initInvDetsTablesGhf(const BFSlater &w)
//  {
//    int norbs = Determinant::norbs;
//    vector<int> workingVec0, workingVec1;
//    concatenateGhf(closedOrbs[0], closedOrbs[1], workingVec0);
//    Eigen::Map<VectorXi> rowTheta(&workingVec0[0], workingVec0.size());
//    concatenateGhf(closedOrbsRef[0], closedOrbsRef[1], workingVec1);
//    Eigen::Map<VectorXi> colTheta(&workingVec1[0], workingVec1.size());
//      
//    MatrixXcd hforbs = w.getHforbs();
//    igl::slice(hforbs, rowTheta, colTheta, theta[0]); 
//    for (int i = 0; i < doublons.size(); i++) {
//      int relIndexA = std::search_n(closedOrbs[0].begin(), closedOrbs[0].end(), 1, doublons[i]) - closedOrbs[0].begin();
//      int relIndexB = std::search_n(closedOrbs[1].begin(), closedOrbs[1].end(), 1, doublons[i]) - closedOrbs[1].begin();
//      for (int j = 0; j < holons.size(); j++) {
//        theta[0].row(relIndexA) += w.bf(i, j) * hforbs.row(holons(j));
//        theta[0].row(closedOrbs[0].size() + relIndexB) += w.bf(i, j) * hforbs.row(norbs + holons(j));
//      }
//    }
//    Eigen::FullPivLU<MatrixXcd> lua(theta[0]);
//    if (lua.isInvertible()) {
//      thetaInv[0] = lua.inverse();
//      thetaDet[0] = lua.determinant();
//    }
//    else {
//      Eigen::Map<VectorXi> v1(&closedOrbs[0][0], closedOrbs[0].size());
//      Eigen::Map<VectorXi> v2(&closedOrbs[1][0], closedOrbs[1].size());
//      cout << "alphaClosed\n" << v1 << endl << endl;
//      cout << "betaClosed\n" << v2 << endl << endl;
//      cout << "col\n" << colTheta << endl << endl;
//      cout << theta << endl << endl;
//      cout << "overlap with theta determinant not invertible" << endl;
//      exit(0);
//    }
//    thetaDet[1] = 1.;
//    rTable[0] = MatrixXcd::Zero(2*norbs, closedOrbs[0].size() + closedOrbs[1].size()); 
//    rTable[0] = w.getHforbs().block(0, 0, 2*norbs, closedOrbs[0].size() + closedOrbs[1].size()) * thetaInv[0];
//    rTable[1] = rTable[0];
//    //makeTableGhf(w, colTheta);
//  }
//
//  void excitationUpdate(const BFSlater &w, const Determinant& excitedDet)
//  {
//    fillOccupancyHelpers(excitedDet);
//    initInvDetsTables(w);
//    //makeTable(w, thetaInv[sz], colClosed, 0, sz);
//    //calcOtherDetsTables(w, sz);
//  }
//
//  void excitationUpdateGhf(const BFSlater &w, const Determinant& excitedDet)
//  {
//    fillOccupancyHelpers(excitedDet);
//    initInvDetsTablesGhf(w);
//    //makeTableGhf(w, colTheta);
//  }
//
//  void getRelIndices(int i, int &relI, int a, int &relA, bool sz) const 
//  {
//    //relI = std::lower_bound(closedOrbs[sz].begin(), closedOrbs[sz].end(), i) - closedOrbs[sz].begin();
//    //relA = std::lower_bound(openOrbs[sz].begin(), openOrbs[sz].end(), a) - openOrbs[sz].begin();
//    int factor = 0;
//    if (hftype == 2 && sz != 0) factor = 1;
//    relI = std::search_n(closedOrbs[sz].begin(), closedOrbs[sz].end(), 1, i) - closedOrbs[sz].begin() + factor * closedOrbs[0].size();
//    //relA = std::search_n(openOrbs[sz].begin(), openOrbs[sz].end(), 1, a) - openOrbs[sz].begin() + factor * openOrbs[0].size();
//    relA = a + factor * Determinant::norbs;
//  }
//
//};

template<>
class WalkerHelper<AGP>
{
  public:
    MatrixXcd thetaInv;                    //inverse of the theta matrix
    std::complex<double> thetaDet;                      //determinant of the theta matrix
    std::array<vector<int>, 2> openOrbs;       //set of open orbitals in the walker
    std::array<vector<int>, 2> closedOrbs;     //set of closed orbitals in the walker
    std::array<MatrixXcd, 3> rTable;            //table used for efficiently

    WalkerHelper() {};
    
    WalkerHelper(const AGP &w, const Determinant &d) 
    {
      //fill the spin strings for the walker and the zeroth reference det
      fillOpenClosedOrbs(d);
      initInvDetsTables(w);
    }
    
    void fillOpenClosedOrbs(const Determinant &d)
    {
      openOrbs[0].clear();
      openOrbs[1].clear();
      closedOrbs[0].clear();
      closedOrbs[1].clear();
      d.getOpenClosedAlphaBeta(openOrbs[0], closedOrbs[0], openOrbs[1], closedOrbs[1]);
    }
    
    void makeTables(const AGP &w)
    {
      Map<VectorXi> rowOpen(&openOrbs[0][0], openOrbs[0].size());
      Map<VectorXi> colClosed(&closedOrbs[1][0], closedOrbs[1].size());
      MatrixXcd openThetaAlpha; 
      igl::slice(w.getPairMat(), rowOpen, colClosed, openThetaAlpha);
      rTable[0] = openThetaAlpha * thetaInv; 
      
      Map<VectorXi> rowClosed(&closedOrbs[0][0], closedOrbs[0].size());
      Map<VectorXi> colOpen(&openOrbs[1][0], openOrbs[1].size());
      MatrixXcd openThetaBeta; 
      igl::slice(w.getPairMat(), rowClosed, colOpen, openThetaBeta);
      MatrixXcd betaTableTranspose = thetaInv * openThetaBeta; 
      rTable[1] = betaTableTranspose.transpose(); 
    
      MatrixXcd openTheta;
      igl::slice(w.getPairMat(), rowOpen, colOpen, openTheta);
      MatrixXcd rtc = rTable[0] * openThetaBeta;
      rTable[2] = openTheta - rtc;
    }
    
    void initInvDetsTables(const AGP &w)
    {
      Eigen::Map<VectorXi> rowClosed(&closedOrbs[0][0], closedOrbs[0].size());
      Eigen::Map<VectorXi> colClosed(&closedOrbs[1][0], closedOrbs[1].size());
      MatrixXcd theta;
      igl::slice(w.getPairMat(), rowClosed, colClosed, theta); 
      Eigen::FullPivLU<MatrixXcd> lua(theta);
      if (lua.isInvertible()) {
        thetaInv = lua.inverse();
        thetaDet = lua.determinant();
      }
      else {
        cout << "pairMat\n" << w.getPairMat() << endl << endl;
        cout << "theta\n" << theta << endl << endl;
        cout << "rowClosed\n" << rowClosed << endl << endl;
        cout << "colClosed\n" << colClosed << endl << endl;
        cout << " overlap with determinant not invertible" << endl;
        exit(0);
      }
      makeTables(w);
    }
    
    void excitationUpdate(const AGP &w, vector<int>& cre, vector<int>& des, bool sz, double parity, const Determinant& excitedDet)
    {
      MatrixXcd invOld = thetaInv;
      std::complex<double> detOld = thetaDet;
      MatrixXcd test1, test2;
      if (sz == 0) {
        Eigen::Map<Eigen::VectorXi> colClosed(&closedOrbs[1][0], closedOrbs[1].size());
        calculateInverseDeterminantWithRowChange(invOld, detOld, test1, thetaInv, thetaDet, test2, cre, des, colClosed, closedOrbs[0], w.getPairMat(), 0);
      }
      if (sz == 1) {
        Eigen::Map<Eigen::VectorXi> rowClosed(&closedOrbs[0][0], closedOrbs[0].size());
        calculateInverseDeterminantWithColumnChange(invOld, detOld, test1, thetaInv, thetaDet, test2, cre, des, rowClosed, closedOrbs[1], w.getPairMat());
      }
      thetaDet *= parity;
      fillOpenClosedOrbs(excitedDet);
      makeTables(w);
    }
    
    void getRelIndices(int i, int &relI, int a, int &relA, bool sz) const 
    {
      relI = std::search_n(closedOrbs[sz].begin(), closedOrbs[sz].end(), 1, i) - closedOrbs[sz].begin();
      relA = std::search_n(openOrbs[sz].begin(), openOrbs[sz].end(), 1, a) - openOrbs[sz].begin();
    }

};

template<>
class WalkerHelper<Pfaffian>
{
  public:
    MatrixXcd thetaInv;                    //inverse of the theta matrix
    complex<double> thetaPfaff;                      //determinant of the theta matrix
    std::array<vector<int>, 2> openOrbs;       //set of open orbitals in the walker
    std::array<vector<int>, 2> closedOrbs;     //set of closed orbitals in the walker
    std::array<MatrixXcd, 2> rTable;
    MatrixXcd fMat;

    WalkerHelper() {};
    
    WalkerHelper(const Pfaffian &w, const Determinant &d) 
    {
      fillOpenClosedOrbs(d);
      int nopen = openOrbs[0].size() + openOrbs[1].size();
      int nclosed = closedOrbs[0].size() + closedOrbs[1].size();
      fMat = MatrixXcd::Zero(nopen * nclosed, nclosed);
      initInvDetsTables(w);
    }
    
    void fillOpenClosedOrbs(const Determinant &d)
    {
      openOrbs[0].clear();
      openOrbs[1].clear();
      closedOrbs[0].clear();
      closedOrbs[1].clear();
      d.getOpenClosedAlphaBeta(openOrbs[0], closedOrbs[0], openOrbs[1], closedOrbs[1]);
    }
    
    void makeTables(const Pfaffian &w)
    {
      int norbs = Determinant::norbs;
      int nopen = openOrbs[0].size() + openOrbs[1].size();
      int nclosed = closedOrbs[0].size() + closedOrbs[1].size();
      Map<VectorXi> openAlpha(&openOrbs[0][0], openOrbs[0].size());
      Map<VectorXi> openBeta(&openOrbs[1][0], openOrbs[1].size());
      VectorXi open(nopen);
      open << openAlpha, (openBeta.array() + norbs).matrix();
      Map<VectorXi> closedAlpha(&closedOrbs[0][0], closedOrbs[0].size());
      Map<VectorXi> closedBeta(&closedOrbs[1][0], closedOrbs[1].size());
      VectorXi closed(nclosed);
      closed << closedAlpha, (closedBeta.array() + norbs).matrix();
      igl::slice(w.getPairMat(), open, closed, fMat);
      rTable[0] = fMat * thetaInv;
      rTable[1] = - rTable[0] * fMat.transpose();

      //MatrixXcd fRow;
      //VectorXi rowSlice(1), colSlice(nclosed);
      //for (int i = 0; i < closed.size(); i++) {
      //  for (int a = 0; a < open.size(); a++) {
      //    colSlice = closed;
      //    rowSlice[0] = open[a];
      //    colSlice[i] = open[a];
      //    igl::slice(w.getPairMat(), rowSlice, colSlice, fRow);
      //    fMat.block(i * open.size() + a, 0, 1, closed.size()) = fRow;
      //  }
      //}
      
      //rTable[0] = fMat * thetaInv; 
      //rTable[1] = - rTable[0] * fMat.transpose(); 
    }
    
    void initInvDetsTables(const Pfaffian &w)
    {
      int norbs = Determinant::norbs;
      int nclosed = closedOrbs[0].size() + closedOrbs[1].size();
      Map<VectorXi> closedAlpha(&closedOrbs[0][0], closedOrbs[0].size());
      Map<VectorXi> closedBeta(&closedOrbs[1][0], closedOrbs[1].size());
      VectorXi closed(nclosed);
      closed << closedAlpha, (closedBeta.array() + norbs).matrix();
      MatrixXcd theta;
      igl::slice(w.getPairMat(), closed, closed, theta); 
      Eigen::FullPivLU<MatrixXcd> lua(theta);
      if (lua.isInvertible()) {
        thetaInv = lua.inverse();
        thetaPfaff = calcPfaffian(theta);
      }
      else {
        cout << "pairMat\n" << w.getPairMat() << endl << endl;
        cout << "theta\n" << theta << endl << endl;
        cout << "colClosed\n" << closed << endl << endl;
        cout << " overlap with determinant not invertible" << endl;
        exit(0);
      }
      makeTables(w);
    }
    
    void excitationUpdate(const Pfaffian &w, int i, int a, bool sz, double parity, const Determinant& excitedDet)
    {
      int tableIndexi, tableIndexa;
      getRelIndices(i, tableIndexi, a, tableIndexa, sz); 
      int norbs = Determinant::norbs;
      int nopen = openOrbs[0].size() + openOrbs[1].size();
      int nclosed = closedOrbs[0].size() + closedOrbs[1].size();
      Map<VectorXi> closedAlpha(&closedOrbs[0][0], closedOrbs[0].size());
      Map<VectorXi> closedBeta(&closedOrbs[1][0], closedOrbs[1].size());
      VectorXi closed(nclosed);
      closed << closedAlpha, (closedBeta.array() + norbs).matrix();
      
      MatrixXcd cInv(2,2);
      cInv << 0, -1,
             1, 0;
      MatrixXcd bMat = MatrixXcd::Zero(nclosed, 2);
      bMat(tableIndexi, 1) = 1;
      VectorXi colSlice(1);
      colSlice[0] = i + sz * norbs;
      MatrixXcd thetaSlice;
      igl::slice(w.getPairMat(), closed, colSlice, thetaSlice);
      //bMat.block(0, 0, nclosed, 1) = - fMat.transpose().block(0, tableIndexi * nopen + tableIndexa, nclosed, 1) - thetaSlice;
      bMat.block(0, 0, nclosed, 1) = - fMat.transpose().block(0, tableIndexa, nclosed, 1) - thetaSlice;
      MatrixXcd invOld = thetaInv;
      MatrixXcd intermediate = (cInv + bMat.transpose() * invOld * bMat).inverse();
      MatrixXcd shuffledThetaInv = invOld - invOld * bMat * intermediate * bMat.transpose() * invOld; 
      
      closed[tableIndexi] = a + sz * norbs; 
      std::vector<int> order(closed.size());
      auto rcopy = closed;
      std::iota(order.begin(), order.end(), 0);
      std::sort(order.begin(), order.end(), [&rcopy](size_t i1, size_t i2) { return rcopy[i1] < rcopy[i2]; });
      Eigen::Map<Eigen::VectorXi> orderVec(&order[0], order.size());
      igl::slice(shuffledThetaInv, orderVec, orderVec, thetaInv);
      
      //thetaPfaff = thetaPfaff * rTable[0](tableIndexi * nopen + tableIndexa, tableIndexi);
      complex<double> pfaffRatio = fMat.row(tableIndexa) * invOld.col(tableIndexi);
      thetaPfaff = thetaPfaff * pfaffRatio;
      thetaPfaff *= parity;
      fillOpenClosedOrbs(excitedDet);
      makeTables(w);
    }
    
    void getRelIndices(int i, int &relI, int a, int &relA, bool sz) const 
    {
      int factor = 0;
      if (sz != 0) factor = 1;
      relI = std::search_n(closedOrbs[sz].begin(), closedOrbs[sz].end(), 1, i) - closedOrbs[sz].begin() + factor * closedOrbs[0].size();
      relA = std::search_n(openOrbs[sz].begin(), openOrbs[sz].end(), 1, a) - openOrbs[sz].begin() + factor * openOrbs[0].size();
    }

};

template<>
class WalkerHelper<CPS>
{
 public:
  std::vector<double> intermediateForEachSpinOrb;
  std::vector<int> commonCorrelators;
  std::vector<std::vector<int> > twoSitesToCorrelator;

  WalkerHelper() {};
  WalkerHelper(CPS& cps, const Determinant& d) {
    int norbs = Determinant::norbs;
    intermediateForEachSpinOrb.resize(norbs*2);
    updateHelper(cps, d, 0, 0, 0);

    if (cps.twoSiteOrSmaller) {
      vector<int> initial(norbs, -1);
      twoSitesToCorrelator.resize(norbs, initial);
    
      int corrIndex = 0;
      for (const auto& corr : cps.cpsArray) {
        if (corr.asites.size() == 2) {
          int site1 = corr.asites[0], site2 = corr.asites[1];
          twoSitesToCorrelator[site1][site2] = corrIndex;
          twoSitesToCorrelator[site2][site1] = corrIndex;
        }
        corrIndex++;
      }

    }
  }

  //these update functions are really init functions
  void updateHelper(CPS& cps, const Determinant& d, int l, int a, bool sz) {
    int norbs = Determinant::norbs;

    for (int i=0; i<2*norbs; i++) {
      intermediateForEachSpinOrb[i] = 1.0;
    
      Determinant dcopy1 = d, dcopy2 = d;
      dcopy1.setocc(i, true);  //make sure this is occupied
      dcopy2.setocc(i, false); //make sure this is unoccupied
    
      const vector<int>& cpsContainingi = cps.mapFromOrbitalToCorrelator[i/2];
      for (const auto& j : cpsContainingi) {
        intermediateForEachSpinOrb[i] *= cps.cpsArray[j].OverlapRatio(dcopy1, dcopy2);
      }
    }
  }
  
  void updateHelper(CPS& cps, const Determinant& d, int l, int m, int a, int b, bool sz) {
    int norbs = Determinant::norbs;

    for (int i=0; i<2*norbs; i++) {
      intermediateForEachSpinOrb[i] = 1.0;
    
      Determinant dcopy1 = d, dcopy2 = d;
      dcopy1.setocc(i, true);  //make sure this is occupied
      dcopy2.setocc(i, false); //make sure this is unoccupied
    
      const vector<int>& cpsContainingi = cps.mapFromOrbitalToCorrelator[i/2];
      for (const auto& j : cpsContainingi) {
        intermediateForEachSpinOrb[i] *= cps.cpsArray[j].OverlapRatio(dcopy1, dcopy2);
      }
    }
  }

  double OverlapRatio(int i, int a, const CPS& cps, const Determinant &dcopy, const Determinant &d) const
  {
    vector<int>& common = const_cast<vector<int>&>(commonCorrelators);
    common.resize(0);

    if (cps.twoSiteOrSmaller) {
      common.push_back(twoSitesToCorrelator[i/2][a/2]);
      if (common[0] == -1) common.resize(0);
    }
    else {
      set_intersection(cps.mapFromOrbitalToCorrelator[i/2].begin(),
                       cps.mapFromOrbitalToCorrelator[i/2].end(),
                       cps.mapFromOrbitalToCorrelator[a/2].begin(),
                       cps.mapFromOrbitalToCorrelator[a/2].end(),
                       back_inserter(common));
      sort(common.begin(), common.end() );
    }
  
    double ovlp = intermediateForEachSpinOrb[a]/intermediateForEachSpinOrb[i];
  
    Determinant dcopy1 = d, dcopy2 = d;
    dcopy1.setocc(i, false); dcopy2.setocc(a, true);

    int previ = -1;
    for (const auto& j : common) {
      ovlp *= cps.cpsArray[j].OverlapRatio(dcopy,d);
      ovlp *= cps.cpsArray[j].OverlapRatio(d, dcopy2);
      ovlp *= cps.cpsArray[j].OverlapRatio(d, dcopy1);
    }
    return ovlp;
  }


  double OverlapRatio(int i, int j, int a, int b, const CPS& cps, const Determinant &dcopy, const Determinant &d) const
  {
    if (!(cps.twoSiteOrSmaller && i/2!=j/2 && i/2!=a/2 && i/2!= b/2 && j/2 != a/2 && j/2 != b/2 && a/2!=b/2))
      return cps.OverlapRatio(i/2, j/2, a/2, b/2, dcopy, d);

    vector<int>& common = const_cast<vector<int>&>(commonCorrelators);
    common.resize(0);
  

    double ovlp = intermediateForEachSpinOrb[a]*intermediateForEachSpinOrb[b]
        /intermediateForEachSpinOrb[i]/intermediateForEachSpinOrb[j];

    Determinant dcopy1 = d, dcopy2 = d;

    {//i j
      dcopy1.setocc(i, false); dcopy2.setocc(j, false);

      int index = twoSitesToCorrelator[i/2][j/2];
      if (index != -1) {
        ovlp *= cps.cpsArray[index].OverlapRatio(dcopy, d);
        ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy2);
        ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy1);
      }
      dcopy1.setocc(i, true); dcopy2.setocc(j, true);
    }
    {//i a
      dcopy1.setocc(i, false); dcopy2.setocc(a, true);

      int index = twoSitesToCorrelator[i/2][a/2];
      if (index != -1) {
        ovlp *= cps.cpsArray[index].OverlapRatio(dcopy, d);
        ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy2);
        ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy1);
      }
      dcopy1.setocc(i, true); dcopy2.setocc(a, false);
    }
    {//i b
      dcopy1.setocc(i, false); dcopy2.setocc(b, true);

      int index = twoSitesToCorrelator[i/2][b/2];
      if (index != -1) {
      ovlp *= cps.cpsArray[index].OverlapRatio(dcopy, d);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy2);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy1);
      }
      dcopy1.setocc(i, true); dcopy2.setocc(b, false);
    }
    {//j a
      dcopy1.setocc(j, false); dcopy2.setocc(a, true);

      int index = twoSitesToCorrelator[j/2][a/2];
      if (index != -1) {
      ovlp *= cps.cpsArray[index].OverlapRatio(dcopy, d);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy2);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy1);
      }
      dcopy1.setocc(j, true); dcopy2.setocc(a, false);
    }
    {//j b
      dcopy1.setocc(j, false); dcopy2.setocc(b, true);

      int index = twoSitesToCorrelator[j/2][b/2];
      if (index != -1) {
      ovlp *= cps.cpsArray[index].OverlapRatio(dcopy, d);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy2);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy1);
      }
      dcopy1.setocc(j, true); dcopy2.setocc(b, false);
    }
    {//a b
      dcopy1.setocc(a, true); dcopy2.setocc(b, true);

      int index = twoSitesToCorrelator[a/2][b/2];
      if (index != -1) {
      ovlp *= cps.cpsArray[index].OverlapRatio(dcopy, d);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy2);
      ovlp *= cps.cpsArray[index].OverlapRatio(d, dcopy1);
      }
      dcopy1.setocc(a, false); dcopy2.setocc(b, false);
    }
  
    return ovlp;
  }
  
  double OverlapRatio(const std::array<unordered_set<int> , 2> &from, const std::array<unordered_set<int> , 2> &to, const CPS& cps) const
  {
    return 0.;
  }

};

template<>
class WalkerHelper<Jastrow>
{
 public:
  std::vector<double> intermediateForEachSpinOrb;

  WalkerHelper() {};
  WalkerHelper(Jastrow& cps, const Determinant& d) {
    int norbs = Determinant::norbs;
    intermediateForEachSpinOrb.resize(norbs*2);
    initHelper(cps, d);
  }

  void initHelper(Jastrow& cps, const Determinant& d) {
    int norbs = Determinant::norbs;

    vector<int> closed;
    vector<int> open;
    d.getOpenClosed(open, closed);
    
    
    for (int i=0; i<2*norbs; i++) {
      intermediateForEachSpinOrb[i] = cps(i,i);
      for (int j=0; j<closed.size(); j++)
        if (closed[j] != i)
          intermediateForEachSpinOrb[i] *= cps(i, closed[j]);
    }
  }
  
  void updateHelper(Jastrow& cps, const Determinant& d, int i, int a, bool sz) {
    i = 2 * i + sz; a = 2 * a + sz;
    int norbs = Determinant::norbs;
    for (int l = 0; l < 2 * norbs; l++) 
      intermediateForEachSpinOrb[l] *= cps(l, a) / cps(l, i);
    intermediateForEachSpinOrb[i] *= cps(i, i);
    intermediateForEachSpinOrb[a] /= cps(a, a);
    //initHelper(cps, d);
  }
  
  void updateHelper(Jastrow& cps, const Determinant& d, int i, int j, int a, int b, bool sz) {
    i = 2 * i + sz; a = 2 * a + sz;
    j = 2 * j + sz; b = 2 * b + sz;
    int norbs = Determinant::norbs;
    for (int l = 0; l < 2 * norbs; l++) 
      intermediateForEachSpinOrb[l] *= cps(l, a) * cps(l, b) / cps(l, i) / cps(l, j);
    intermediateForEachSpinOrb[i] *= cps(i, i);
    intermediateForEachSpinOrb[a] /= cps(a, a);
    intermediateForEachSpinOrb[j] *= cps(j, j);
    intermediateForEachSpinOrb[b] /= cps(b, b);
    //initHelper(cps, d);
  }


  double OverlapRatio(int i, int a, const Jastrow& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    return intermediateForEachSpinOrb[a]/intermediateForEachSpinOrb[i]/cps(i,a);
  }
  
  double OverlapRatio(int i, int j, int a, int b, const Jastrow& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    return intermediateForEachSpinOrb[a]*intermediateForEachSpinOrb[b]*cps(a,b)*cps(i,j)
        /intermediateForEachSpinOrb[i]/intermediateForEachSpinOrb[j]/
        cps(i,a)/cps(j,a)/cps(i,b)/cps(j,b);
  }
  
  double OverlapRatio(const std::array<unordered_set<int> , 2> &from, const std::array<unordered_set<int> , 2> &to, const Jastrow& cps) const
  {
    vector<int> fromSpin, toSpin;
    fromSpin.clear(); toSpin.clear();
    double ratio = 1.;
    for (int sz = 0; sz < 2; sz++) {//iterate over spins
      auto itFrom = from[sz].begin();
      auto itTo = to[sz].begin();
      for (int n = 0; n < from[sz].size(); n++) {//iterate over excitations
        int i = 2 * (*itFrom) + sz, a = 2 * (*itTo) + sz;
        itFrom = std::next(itFrom); itTo = std::next(itTo);
        ratio *= intermediateForEachSpinOrb[a] / intermediateForEachSpinOrb[i] / cps(i, a);
        for (int p = 0; p < fromSpin.size(); p++) {
          ratio *= cps(i, fromSpin[p]) * cps(a, toSpin[p]) / cps(i, toSpin[p]) / cps(fromSpin[p], a);
        }
        fromSpin.push_back(i);
        toSpin.push_back(a);
      }
    }
    return ratio;
  }
};  


template<>
class WalkerHelper<SJastrow>
{
 public:
  std::vector<double> intermediateForEachOrb;
  VectorXi occ;

  WalkerHelper() {};
  WalkerHelper(SJastrow& cps, const Determinant& d) {
    int norbs = Determinant::norbs;
    intermediateForEachOrb.resize(norbs);
    initHelper(cps, d);
  }

  void initHelper(SJastrow& cps, const Determinant& d) {
    int norbs = Determinant::norbs;
    vector<int> closed;
    vector<int> open;
    d.getOpenClosed(open, closed);
    occ = VectorXi::Zero(norbs);
    for (int i = 0; i < closed.size(); i++) occ[closed[i]/2]++;
    
    for (int i = 0; i < norbs; i++) {
      intermediateForEachOrb[i] = 0.;
      for (int j = 0; j < norbs; j++)
        if (j != i) intermediateForEachOrb[i] += cps(i, j) * occ[j];
    }
  }
  
  void updateHelper(SJastrow& cps, const Determinant& d, int i, int a, bool sz) {
    int norbs = Determinant::norbs;
    for (int l = 0; l < norbs; l++) 
      intermediateForEachOrb[l] += (cps(l, a) - cps(l, i));
    intermediateForEachOrb[i] += cps(i, i);
    intermediateForEachOrb[a] -= cps(a, a);
    occ[i] -= 1;
    occ[a] += 1;
  }
  
  void updateHelper(SJastrow& cps, const Determinant& d, int i, int j, int a, int b, bool sz) {
    int norbs = Determinant::norbs;
    for (int l = 0; l < norbs; l++) 
      intermediateForEachOrb[l] += (cps(l, a) + cps(l, b) - cps(l, i) - cps(l, j));
    intermediateForEachOrb[i] += cps(i, i);
    intermediateForEachOrb[a] -= cps(a, a);
    intermediateForEachOrb[j] += cps(j, j);
    intermediateForEachOrb[b] -= cps(b, b);
    occ[i] -= 1;
    occ[j] -= 1;
    occ[a] += 1;
    occ[b] += 1;
  }


  double OverlapRatio(int i, int a, const SJastrow& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    return exp(intermediateForEachOrb[a/2] - cps(i/2, a/2) - intermediateForEachOrb[i/2] + (2 * occ[a/2] + 1) * cps(a/2, a/2) - (2 * occ[i/2] - 1) * cps(i/2, i/2));
  }
  
  double OverlapRatio(int i, int j, int a, int b, const SJastrow& cps,
                      const Determinant &dcopy, const Determinant &d) const 
  {
    //VectorXi occCopy = occ;
    //vector<double> intCopy = intermediateForEachOrb;
    double exponent = intermediateForEachOrb[a/2] - cps(i/2, a/2) - intermediateForEachOrb[i/2] + (2 * occ[a/2] + 1) * cps(a/2, a/2) - (2 * occ[i/2] - 1) * cps(i/2, i/2);
    double intCopyj = intermediateForEachOrb[j/2];
    double intCopyb = intermediateForEachOrb[b/2];
    int occCopyj = occ[j/2];
    int occCopyb = occ[b/2];
    //occCopy[i/2]--; occCopy[a/2]++;
    //intCopy[j/2] += cps(j/2, a/2) - cps(j/2, i/2);
    //intCopy[b/2] += cps(b/2, a/2) - cps(b/2, i/2);
    intCopyj += cps(j/2, a/2) - cps(j/2, i/2);
    intCopyb += cps(b/2, a/2) - cps(b/2, i/2);
    //if (i/2 == j/2) intCopy[j/2] += cps(i/2, i/2);
    //if (a/2 == j/2) intCopy[j/2] -= cps(a/2, a/2);
    //if (i/2 == b/2) intCopy[b/2] += cps(i/2, i/2);
    //if (a/2 == b/2) intCopy[b/2] -= cps(a/2, a/2);
    if (i/2 == j/2) { intCopyj += cps(i/2, i/2); occCopyj--; }
    if (a/2 == j/2) { intCopyj -= cps(a/2, a/2); occCopyj++; }
    if (i/2 == b/2) { intCopyb += cps(i/2, i/2); occCopyb--; }
    if (a/2 == b/2) { intCopyb -= cps(a/2, a/2); occCopyb++; }
    //exponent += intCopy[b/2] - cps(j/2, b/2) - intCopy[j/2] + (2 * occCopy[b/2] + 1) * cps(b/2, b/2) - (2 * occCopy[j/2] - 1) * cps(j/2, j/2);
    exponent += intCopyb - cps(j/2, b/2) - intCopyj + (2 * occCopyb + 1) * cps(b/2, b/2) - (2 * occCopyj - 1) * cps(j/2, j/2);

    return exp(exponent);
  }
  
  double OverlapRatio(const std::array<unordered_set<int> , 2> &from, const std::array<unordered_set<int> , 2> &to, const SJastrow& cps) const
  {
    return 0.;
  }
};  


template<>
class WalkerHelper<Gutzwiller>
{
 public:

  WalkerHelper() {};
  WalkerHelper(Gutzwiller& gutz, const Determinant& d) {}

  void updateHelper(Gutzwiller& gutz, const Determinant& d, int i, int a, bool sz) {}
  
  void updateHelper(Gutzwiller& gutz, const Determinant& d, int i, int j, int a, int b, bool sz) {}

  double OverlapRatio(int i, int a, const Gutzwiller& gutz,
                      const Determinant &dcopy, const Determinant &d) const
  {
    double ratio = 1;
    if (i % 2 == 0) { 
      if (d.getoccB(i/2)) ratio /= gutz.g(i/2);  
      if (d.getoccB(a/2)) ratio *= gutz.g(a/2);
    }
    else { 
      if (d.getoccA(i/2)) ratio /= gutz.g(i/2);  
      if (d.getoccA(a/2)) ratio *= gutz.g(a/2);
    }
    return ratio;
    //return gutz.OverlapRatio(dcopy, d);
  }
  
  double OverlapRatio(int i, int j, int a, int b, const Gutzwiller& gutz,
                      const Determinant &dcopy, const Determinant &d) const
  {
    double ratio = 1;
    if (i % 2 == 0) { 
      if (d.getoccB(i/2)) ratio /= gutz.g(i/2);  
      if (d.getoccB(a/2)) ratio *= gutz.g(a/2);
    }
    else { 
      if (d.getoccA(i/2)) ratio /= gutz.g(i/2);  
      if (d.getoccA(a/2)) ratio *= gutz.g(a/2);
    }
    Determinant dExc = d;
    dExc.setocc(i, false); dExc.setocc(a, true);
    if (j % 2 == 0) { 
      if (dExc.getoccB(j/2)) ratio /= gutz.g(j/2);  
      if (dExc.getoccB(b/2)) ratio *= gutz.g(b/2);
    }
    else { 
      if (dExc.getoccA(j/2)) ratio /= gutz.g(j/2);  
      if (dExc.getoccA(b/2)) ratio *= gutz.g(b/2);
    }
    return ratio;
    //return gutz.OverlapRatio(dcopy, d);
  }
  
  double OverlapRatio(const std::array<unordered_set<int> , 2> &from, const std::array<unordered_set<int> , 2> &to, const Gutzwiller& gutz) const
  {
    return 0.;
  }
};  

template<>
class WalkerHelper<RBM>
{
 public:
  //the intermediate data is stored in the wfn, this class initializes and updates it

  WalkerHelper() {};
  WalkerHelper(RBM& cps, const Determinant& d) {
    int norbs = Determinant::norbs;
    //cps.bwn = ArrayXd::Zero(cps.numHidden);
    cps.bwn = VectorXd::Zero(cps.numHidden);
    //cps.intermediates[0] = ArrayXXd::Zero(norbs, norbs);
    //cps.intermediates[1] = ArrayXXd::Zero(norbs, norbs);
    initHelper(cps, d);
  }

  void initHelper(RBM& cps, const Determinant& d) {
    int norbs = Determinant::norbs;
    vector<int> closed;
    vector<int> open;
    d.getOpenClosed(open, closed);
    
    //bwn
    cps.bwn = cps.bVec;
    for (int j = 0; j < closed.size(); j++) {
      cps.bwn += cps.wMat.col(closed[j]);
    }
    cps.coshbwn = cosh(cps.bwn.array()).prod();

    //intermediates
    //ArrayXd tanhbwn = tanh(cps.bwn);
    //for (int i = commrank; i < (norbs * (norbs-1)) / 2; i += commsize) {
    //  //calc row (occupied) and column (unoccupied) up spatial orb indices
    //  int occ = floor(0.5 + sqrt(1 + 8*i) / 2), unocc = i % occ;//bottom half
    //  ArrayXd wDiff = cps.wMat.col(2 * unocc) - cps.wMat.col(2 * occ);
    //  ArrayXd coshWDiff = cosh(wDiff), sinhWDiff = sinh(wDiff);
    //  cps.intermediates[0](occ, unocc) = (coshWDiff + tanhbwn * sinhWDiff).prod();
    //  cps.intermediates[0](unocc, occ) = (coshWDiff - tanhbwn * sinhWDiff).prod();//bottom half
    //  wDiff = cps.wMat.col(2 * unocc + 1) - cps.wMat.col(2 * occ + 1);
    //  coshWDiff = cosh(wDiff); sinhWDiff = sinh(wDiff);
    //  cps.intermediates[1](occ, unocc) = (coshWDiff + tanhbwn * sinhWDiff).prod();
    //  cps.intermediates[1](unocc, occ) = (coshWDiff - tanhbwn * sinhWDiff).prod();
    //}
//#ifndef SERIAL
//  MPI_Allreduce(MPI_IN_PLACE, cps.intermediates[0].data(), norbs*norbs, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//  MPI_Allreduce(MPI_IN_PLACE, cps.intermediates[1].data(), norbs*norbs, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
//#endif
  }
  
  void updateHelper(RBM& cps, const Determinant& d, int i, int a, bool sz) {
    i = 2 * i + sz; a = 2 * a + sz;
    cps.bwn += cps.wMat.col(a) - cps.wMat.col(i);
    cps.coshbwn = cosh(cps.bwn.array()).prod();
  }
  
  void updateHelper(RBM& cps, const Determinant& d, int i, int j, int a, int b, bool sz) {
    i = 2 * i + sz; a = 2 * a + sz;
    j = 2 * j + sz; b = 2 * b + sz;
    cps.bwn += cps.wMat.col(a) - cps.wMat.col(i) + cps.wMat.col(b) - cps.wMat.col(j);
    cps.coshbwn = cosh(cps.bwn.array()).prod();
  }

  double OverlapRatio(int i, int a, const RBM& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    double aFac = exp(cps.aVec(a) - cps.aVec(i));
    VectorXd bwnp = cps.bwn + cps.wMat.col(a) - cps.wMat.col(i);
    return aFac * cosh(bwnp.array()).prod() / cps.coshbwn;
  }
  
  double OverlapRatio(int i, int j, int a, int b, const RBM& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    double aFac = exp(cps.aVec(a) - cps.aVec(i) + cps.aVec(b) - cps.aVec(j));
    VectorXd bwnp = cps.bwn + cps.wMat.col(a) - cps.wMat.col(i) + cps.wMat.col(b) - cps.wMat.col(j);
    return aFac * cosh(bwnp.array()).prod() / cps.coshbwn;
  }
  
  double OverlapRatio(const std::array<unordered_set<int> , 2> &from, const std::array<unordered_set<int> , 2> &to, const RBM& cps) const
  {
    return 0.;
  }
};  

template<>
class WalkerHelper<JRBM>
{
 public:
  WalkerHelper<Jastrow> jastrowHelper;
  WalkerHelper<RBM> RBMHelper;

  WalkerHelper() {};
  WalkerHelper(JRBM& cps, const Determinant& d) {
    jastrowHelper = WalkerHelper<Jastrow>(cps.jastrow, d);
    RBMHelper = WalkerHelper<RBM>(cps.rbm, d);
  }

  void initHelper(JRBM& cps, const Determinant& d) {
    jastrowHelper.initHelper(cps.jastrow, d);
    RBMHelper.initHelper(cps.rbm, d);
  }
  
  void updateHelper(JRBM& cps, const Determinant& d, int i, int a, bool sz) {
    jastrowHelper.updateHelper(cps.jastrow, d, i, a, sz);
    RBMHelper.updateHelper(cps.rbm, d, i, a, sz);
  }
  
  void updateHelper(JRBM& cps, const Determinant& d, int i, int j, int a, int b, bool sz) {
    jastrowHelper.updateHelper(cps.jastrow, d, i, j, a, b, sz);
    RBMHelper.updateHelper(cps.rbm, d, i, j, a, b, sz);
  }

  double OverlapRatio(int i, int a, const JRBM& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    return jastrowHelper.OverlapRatio(i, a, cps.jastrow, dcopy, d) * RBMHelper.OverlapRatio(i, a, cps.rbm, dcopy, d);
  }
  
  double OverlapRatio(int i, int j, int a, int b, const JRBM& cps,
                      const Determinant &dcopy, const Determinant &d) const
  {
    return jastrowHelper.OverlapRatio(i, j, a, b, cps.jastrow, dcopy, d) * RBMHelper.OverlapRatio(i, j, a, b, cps.rbm, dcopy, d);
  }
  
  double OverlapRatio(const std::array<unordered_set<int> , 2> &from, const std::array<unordered_set<int> , 2> &to, const JRBM& cps) const
  {
    return 0.;
  }
};  
#endif
