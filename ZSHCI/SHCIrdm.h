/*
  Developed by Sandeep Sharma with contributions from James E. T. Smith and Adam
  A. Holmes, 2017 Copyright (c) 2017, Sandeep Sharma

  This file is part of DICE.

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  this program. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef SHCI_RDM_H
#define SHCI_RDM_H
#include <Eigen/Dense>
#include <boost/serialization/serialization.hpp>
#include <list>
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include "global.h"

using namespace std;
using namespace Eigen;
class Determinant;
class HalfDet;
class oneInt;
class twoInt;
class twoIntHeatBath;
class twoIntHeatBathSHM;
class schedule;
namespace SHCISortMpiUtils {
class StitchDEH;
};

namespace SHCIrdm {
void makeRDM(int*& AlphaMajorToBetaLen, vector<int*>& AlphaMajorToBeta,
             vector<int*>& AlphaMajorToDet, int*& BetaMajorToAlphaLen,
             vector<int*>& BetaMajorToAlpha, vector<int*>& BetaMajorToDet,
             int*& SinglesFromAlphaLen, vector<int*>& SinglesFromAlpha,
             int*& SinglesFromBetaLen, vector<int*>& SinglesFromBeta,
             Determinant* Dets, int DetsSize, int Norbs, int nelec,
             CItype* cibra, CItype* ciket, MatrixXx& s2RDM);

void save1RDM(schedule& schd, MatrixXx& s1RDM, MatrixXx& oneRDM, int root);
void saveRDM(schedule& schd, MatrixXx& s2RDM, MatrixXx& twoRDM, int root);
void save1RDM_bin(schedule& schd, MatrixXx& oneRDM, int root);
void saveRDM_bin(schedule& schd, MatrixXx& twoRDM, int root);
void loadRDM(schedule& schd, MatrixXx& s2RDM, MatrixXx& twoRDM, int root);
void save3RDM(schedule& schd, MatrixXx& threeRDM, MatrixXx& s3RDM, int root,
              size_t norbs);
void save4RDM(schedule& schd, MatrixXx& fourRDM, MatrixXx& s4RDM, int root,
              int norbs);
void load3RDM(schedule& schd, MatrixXx& s3RDM, int root);

void EvaluateRDM(vector<vector<int> >& connections, Determinant* Dets,
                 int DetsSize, CItype* cibra, CItype* ciket,
                 vector<vector<size_t> >& orbDifference, int nelec,
                 schedule& schd, int root, MatrixXx& s2RDM, MatrixXx& twoRDM);

void EvaluateOneRDM(vector<vector<int> >& connections, Determinant* Dets,
                    int DetsSize, CItype* cibra, CItype* ciket,
                    vector<vector<size_t> >& orbDifference, int nelec,
                    schedule& schd, int root, MatrixXx& oneRDM,
                    MatrixXx& s1RDM);

void UpdateRDMResponsePerturbativeDeterministic(
    Determinant* Dets, int DetsSize, CItype* ci, double& E0, oneInt& I1,
    twoInt& I2, schedule& schd, double coreE, int nelec, int norbs,
    SHCISortMpiUtils::StitchDEH& uniqueDEH, int root, double& Psi1Norm,
    MatrixXx& s2RDM, MatrixXx& twoRDM);

void UpdateRDMPerturbativeDeterministic(
    vector<Determinant>& Dets, MatrixXx& ci, double& E0, oneInt& I1, twoInt& I2,
    schedule& schd, double coreE, int nelec, int norbs,
    std::vector<SHCISortMpiUtils::StitchDEH>& uniqueDEH, int root,
    MatrixXx& s2RDM, MatrixXx& twoRDM);

void populateSpinRDM(int &i, int &j, int &k, int &l,
                     MatrixXx &RDM, CItype value,
                     int norbs);
void populateSpatialRDM(int& i, int& j, int& k, int& l, MatrixXx& s2RDM,
                        CItype value, int& nSpatOrbs);

double ComputeEnergyFromSpinRDM(int norbs, int nelec, oneInt& I1, twoInt& I2,
                                double coreE, MatrixXx& twoRDM);

double ComputeEnergyFromSpatialRDM(int norbs, int nelec, oneInt& I1, twoInt& I2,
                                   double coreE, MatrixXx& twoRDM);

void getUniqueIndices(Determinant& bra, Determinant& ket, vector<int>& cIndices,
                      vector<int>& dIndices);

void popSpatial3RDM(vector<int>& cs, vector<int>& ds, CItype value,
                    size_t& norbs, MatrixXx& s3RDM);

void popSpatial4RDM(vector<int>& cs, vector<int>& ds, CItype value, int& nSOs,
                    MatrixXx& s4RDM);

void Evaluate3RDM(Determinant* Dets, int DetsSize, CItype* cibra, CItype* ciket,
                  int nelec, schedule& schd, int root, MatrixXx& threeRDM,
                  MatrixXx& s3RDM);

void Evaluate4RDM(Determinant* Dets, int DetsSize, CItype* cibra, CItype* ciket,
                  int nelec, schedule& schd, int root, MatrixXx& fourRDM,
                  MatrixXx& s4RDM);
};  // namespace SHCIrdm

#endif
