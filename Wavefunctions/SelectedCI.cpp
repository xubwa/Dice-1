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
#include "Determinants.h"
#include "SelectedCI.h"
#include "workingArray.h"
#include "integral.h"

SelectedCI::SelectedCI() {}

void SelectedCI::readWave() {
  int norbs = Determinant::norbs;
  int nalpha = Determinant::nalpha;
  int nbeta = Determinant::nbeta;

  if (commrank == 0) cout << "Reading active space determinants\n";
  shortSimpleDet NULL_SIMPLE_DET;
  //for (int i=0; i++; i<2*innerDetLen)
  //  NULL_SIMPLE_DET[i] = 0.0;
  DetsMap.set_empty_key(NULL_SIMPLE_DET);

  if (boost::iequals(schd.determinantFile, "") || boost::iequals(schd.determinantFile, "bestDet"))
  {
    Determinant det;

    for (int i = 0; i < nalpha; i++)
      det.setoccA(i, true);
    for (int i = 0; i < nbeta; i++)
      det.setoccB(i, true);
    DetsMap[det.getShortSimpleDet()] = 1.0;
    bestDeterminant = det;
  }
  else
  {
    ifstream dump(schd.determinantFile.c_str());
    int index = 0;
    double bestCoeff = 0.0;

    int orbsToLoopOver;
    int offset;
    Determinant detCAS;

    if (schd.detsInCAS) {
      orbsToLoopOver = schd.nciAct;
      offset = schd.nciCore;
      // Create determinant with all core orbitals occupied
      for (int i=0; i<schd.nciCore; i++) {
        detCAS.setoccA(i, true);
        detCAS.setoccB(i, true);
      }
    } else {
      orbsToLoopOver = Determinant::norbs;
      offset = 0;
    }

    while (dump.good())
    {
      std::string Line;
      std::getline(dump, Line);

      boost::trim_if(Line, boost::is_any_of(", \t\n"));
      
      vector<string> tok;
      boost::split(tok, Line, boost::is_any_of(", \t\n"), boost::token_compress_on);

      if (tok.size() > 2 )
      {
        double ci = atof(tok[0].c_str());
        Determinant det ;

        if (schd.detsInCAS) det = detCAS;

        for (int i=0; i<orbsToLoopOver; i++)
        {
          if (boost::iequals(tok[1+i], "2")) 
          {
            det.setoccA(i+offset, true);
            det.setoccB(i+offset, true);
          }
          else if (boost::iequals(tok[1+i], "a")) 
          {
            det.setoccA(i+offset, true);
            det.setoccB(i+offset, false);
          }
          if (boost::iequals(tok[1+i], "b")) 
          {
            det.setoccA(i+offset, false);
            det.setoccB(i+offset, true);
          }
          if (boost::iequals(tok[1+i], "0")) 
          {
            det.setoccA(i+offset, false);
            det.setoccB(i+offset, false);
          }
        }
        
        DetsMap[det.getShortSimpleDet()] = ci;
        if (abs(ci) > abs(bestCoeff)) {
          bestCoeff = ci;
          bestDeterminant = det;
        }
      }
    }

  }
  if (commrank == 0) {
    cout << "Finished reading determinants, hash table bucket count: " << DetsMap.bucket_count() << endl;
  }
}

//assuming bestDeterminant is an active space det, so no excitedOrbs
void SelectedCI::initWalker(SimpleWalker &walk) {
  int norbs = Determinant::norbs;
  walk.d = bestDeterminant;
  walk.excitedHoles.clear();
  walk.excitedOrbs.clear();
  for (int i = 0; i < schd.nciCore; i++) {
    if (!walk.d.getoccA(i)) walk.excitedHoles.insert(2*i);
    if (!walk.d.getoccB(i)) walk.excitedHoles.insert(2*i+1);
  }
  for (int i = schd.nciCore + schd.nciAct; i < Determinant::norbs; i++) {
    if (walk.d.getoccA(i)) walk.excitedOrbs.insert(2*i);
    if (walk.d.getoccB(i)) walk.excitedOrbs.insert(2*i+1);
  }
  walk.getExcitationClass();
  
  //vector<int> open;
  //vector<int> closed;
  //walk.d.getOpenClosed(open, closed);
  //walk.energyIntermediates[0]= VectorXd::Zero(norbs);
  //walk.energyIntermediates[1]= VectorXd::Zero(norbs);
  //for (int i = 0; i < norbs; i++) {
  //  for (int j = 0; j < closed.size(); j++) {
  //    walk.energyIntermediates[0][i] += I2.Direct(i, closed[j]/2) + I1(2*i, 2*i);
  //    walk.energyIntermediates[1][i] += I2.Direct(i, closed[j]/2) + I1(2*i, 2*i);
  //    walk.energyIntermediates[closed[j] % 2][i] -= I2.Exchange(i, closed[j]/2);
  //  }
  //}
}

void SelectedCI::initWalker(SimpleWalker &walk, Determinant& d) {
  int norbs = Determinant::norbs;
  walk.d = d;
  walk.excitedHoles.clear();
  walk.excitedOrbs.clear();
  for (int i = 0; i < schd.nciCore; i++) {
    if (!walk.d.getoccA(i)) walk.excitedHoles.insert(2*i);
    if (!walk.d.getoccB(i)) walk.excitedHoles.insert(2*i+1);
  }
  for (int i = schd.nciCore + schd.nciAct; i < Determinant::norbs; i++) {
    if (d.getoccA(i)) walk.excitedOrbs.insert(2*i);
    if (d.getoccB(i)) walk.excitedOrbs.insert(2*i+1);
  }
  walk.getExcitationClass();
 
  //vector<int> open;
  //vector<int> closed;
  //walk.d.getOpenClosed(open, closed);
  //walk.energyIntermediates[0]= VectorXd::Zero(norbs);
  //walk.energyIntermediates[1]= VectorXd::Zero(norbs);
  //for (int i = 0; i < norbs; i++) {
  //  for (int j = 0; j < closed.size(); j++) {
  //    walk.energyIntermediates[0][i] += I2.Direct(i, closed[j]/2) + I1(2*i, 2*i);
  //    walk.energyIntermediates[1][i] += I2.Direct(i, closed[j]/2) + I1(2*i, 2*i);
  //    walk.energyIntermediates[closed[j] % 2][i] -= I2.Exchange(i, closed[j]/2);
  //  }
  //}
}

//only used in deterministic calcs
double SelectedCI::getOverlapFactor(SimpleWalker& walk, Determinant& dcopy, bool doparity) {
  auto it1 = DetsMap.find(walk.d.getShortSimpleDet());
  auto it2 = DetsMap.find(dcopy.getShortSimpleDet());
  if (it1 != DetsMap.end() && it2 != DetsMap.end())
    return it2->second/it1->second;
  else
    return 0.0;
}

double SelectedCI::getOverlapFactor(int I, int A, SimpleWalker& walk, bool doparity) {
  Determinant dcopy = walk.d;
  dcopy.setocc(I, false);
  dcopy.setocc(A, true);
  auto it1 = DetsMap.find(walk.d.getShortSimpleDet());
  auto it2 = DetsMap.find(dcopy.getShortSimpleDet());
  if (it1 != DetsMap.end() && it2 != DetsMap.end())
    return it2->second/it1->second;
  else
    return 0.0;
}

double SelectedCI::getOverlapFactor(int I, int J, int A, int B,
                                    SimpleWalker& walk, bool doparity) {

  // Single excitation
  if (J == 0 && B == 0) return getOverlapFactor(I, A, walk, doparity);

  Determinant dcopy = walk.d;
  dcopy.setocc(I, false);
  dcopy.setocc(A, true);
  dcopy.setocc(J, false);
  dcopy.setocc(B, true);
  auto it1 = DetsMap.find(walk.d.getShortSimpleDet());
  auto it2 = DetsMap.find(dcopy.getShortSimpleDet());
  if (it1 != DetsMap.end() && it2 != DetsMap.end())
    return it2->second/it1->second;
  else
    return 0.0;
}
  
double SelectedCI::Overlap(SimpleWalker& walk) {
  auto it1 = DetsMap.find(walk.d.getShortSimpleDet());
  if (it1 != DetsMap.end())
    return it1->second;
  else
    return 0.0;
}

double SelectedCI::Overlap(Determinant& d) {
  auto it1 = DetsMap.find(d.getShortSimpleDet());
  if (it1 != DetsMap.end())
    return it1->second;
  else
    return 0.0;
}

inline double SelectedCI::Overlap(shortSimpleDet& d) {
  auto it1 = DetsMap.find(d);
  if (it1 != DetsMap.end())
    return it1->second;
  else
    return 0.0;
}

void SelectedCI::OverlapWithGradient(SimpleWalker &walk,
                                     double &factor,
                                     Eigen::VectorXd &grad) {
  auto it1 = DetsMap.find(walk.d.getShortSimpleDet());
  if (it1 != DetsMap.end())
    grad[it1->second] = 1.0;
}

// This version of HamAndOvlp is the standard version, appropriate when
// performing VMC in the usual way, using a SelectedCI wave function.
// This function calculations both ovlp and ham. ovlp is the overlap of
// walk.d with the selected CI wave function. ham is the local energy
// on determinant walk.d, including the 1/ovlp factor.
void SelectedCI::HamAndOvlp(SimpleWalker &walk, double &ovlp, double &ham,
                            workingArray& work, double epsilon)
{
  int norbs = Determinant::norbs;

  ovlp = Overlap(walk);
  ham = walk.d.Energy(I1, I2, coreE);

  work.setCounterToZero();
  generateAllScreenedSingleExcitation(walk.d, epsilon, schd.screen,
                                      work, false);
  generateAllScreenedDoubleExcitation(walk.d, epsilon, schd.screen,
                                      work, false);

  //loop over all the screened excitations
  if (schd.debug) cout << "eloc excitations\nphi0  d.energy " << ham << endl;
  for (int i=0; i<work.nExcitations; i++) {
    int ex1 = work.excitation1[i], ex2 = work.excitation2[i];
    double tia = work.HijElement[i];

    int I = ex1 / 2 / norbs, A = ex1 - 2 * norbs * I;
    int J = ex2 / 2 / norbs, B = ex2 - 2 * norbs * J;

    // Find the parity
    double parity = 1.;
    Determinant dcopy = walk.d;
    parity *= dcopy.parity(A/2, I/2, I%2);
    dcopy.setocc(I, false);
    dcopy.setocc(A, true);
    if (ex2 != 0) {
      parity *= dcopy.parity(B/2, J/2, J%2);
      dcopy.setocc(J, false);
      dcopy.setocc(B, true);
    }

    shortSimpleDet dcopySimple = dcopy.getShortSimpleDet();
    double ovlpcopy = Overlap(dcopySimple);
    double ovlpRatio = ovlpcopy / ovlp;

    ham += parity * tia * ovlpRatio;
    work.ovlpRatio[i] = ovlpRatio;
  }
  if (schd.debug) cout << endl;
}

void SelectedCI::HamAndOvlpAndSVTotal(SimpleWalker &walk, double &ovlp,
                                      double &ham, double& SVTotal,
                                      workingArray& work, double epsilon)
{
  int norbs = Determinant::norbs;

  ovlp = Overlap(walk);
  ham = walk.d.Energy(I1, I2, coreE);
  SVTotal = 0.0;

  work.setCounterToZero();
  generateAllScreenedSingleExcitation(walk.d, epsilon, schd.screen, work, false);
  generateAllScreenedDoubleExcitation(walk.d, epsilon, schd.screen, work, false);

  // Loop over all the screened excitations
  for (int i=0; i<work.nExcitations; i++) {
    int ex1 = work.excitation1[i];
    int ex2 = work.excitation2[i];
    double tia = work.HijElement[i];

    int I = ex1 / 2 / norbs, A = ex1 - 2 * norbs * I;
    int J = ex2 / 2 / norbs, B = ex2 - 2 * norbs * J;

    // Find the parity
    double parity = 1.;
    Determinant dcopy = walk.d;
    parity *= dcopy.parity(A/2, I/2, I%2);
    dcopy.setocc(I, false);
    dcopy.setocc(A, true);
    if (ex2 != 0) {
      parity *= dcopy.parity(B/2, J/2, J%2);
      dcopy.setocc(J, false);
      dcopy.setocc(B, true);
    }

    shortSimpleDet dcopySimple = dcopy.getShortSimpleDet();
    double ovlpcopy = Overlap(dcopySimple);
    double ovlpRatio = ovlpcopy / ovlp;

    double contrib = parity * tia * ovlpRatio;
    ham += contrib;
    if (contrib > 0.0) {
      SVTotal += contrib;
    }

    work.ovlpRatio[i] = ovlpRatio;
  }
}

// This version of HamAndOvlp is used for MRCI and NEVPT calculations,
// where excitations occur into the first-order interacting space, but
// the selected CI wave function only has non-zero coefficients
// within the complete active space.
// *IMPORTANT* - ham here is <n|H|phi0>, *not* the ratio, to avoid out
// of active space singularitites. Also, ovlp = ham when ham is calculated.
void SelectedCI::HamAndOvlp(SimpleWalker &walk, double &ovlp, double &ham,
                  workingArray& work, bool dontCalcEnergy) {

  walk.getExcitationClass();

  int norbs = Determinant::norbs;
  if (dontCalcEnergy) {
    ovlp = Overlap(walk);
    return;//ham *= ovlp;
  }
  else ham = 0.;//ham = ovlp * walk.d.Energy(I1, I2, coreE);

  work.setCounterToZero();

  if (walk.excitation_class == 0) {
    generateAllScreenedSingleExcitationsCAS_0h0p(walk.d, schd.epsilon, schd.screen, work, false);
    generateAllScreenedDoubleExcitationsCAS_0h0p(walk.d, schd.epsilon, work);
  }
  else if (walk.excitation_class == 1) {
    generateAllScreenedSingleExcitationsCAS_0h1p(walk.d, schd.epsilon, schd.screen,
                                                 work, *walk.excitedOrbs.begin(), false);
    generateAllScreenedDoubleExcitationsCAS_0h1p(walk.d, schd.epsilon, work, *walk.excitedOrbs.begin());
  }
  else if (walk.excitation_class == 2) {
    generateAllScreenedExcitationsCAS_0h2p(walk.d, schd.epsilon, work, *walk.excitedOrbs.begin(),
                                           *std::next(walk.excitedOrbs.begin()));
  }
  else if (walk.excitation_class == 3) {
    generateAllScreenedSingleExcitationsCAS_1h0p(walk.d, schd.epsilon, schd.screen,
                                                 work, *walk.excitedHoles.begin(), false);
    generateAllScreenedDoubleExcitationsCAS_1h0p(walk.d, schd.epsilon, work, *walk.excitedHoles.begin());
  }
  else if (walk.excitation_class == 4) {
    generateAllScreenedSingleExcitationsCAS_1h1p(walk.d, schd.epsilon, schd.screen, work,
                                                 *walk.excitedOrbs.begin(), *walk.excitedHoles.begin(), false);
    generateAllScreenedDoubleExcitationsCAS_1h1p(walk.d, schd.epsilon, work,
                                                 *walk.excitedOrbs.begin(), *walk.excitedHoles.begin());
  }
  else if (walk.excitation_class == 5) {
    generateAllScreenedExcitationsCAS_1h2p(walk.d, schd.epsilon, work, *walk.excitedOrbs.begin(),
                                           *std::next(walk.excitedOrbs.begin()), *walk.excitedHoles.begin());
  }
  else if (walk.excitation_class == 6) {
    generateAllScreenedExcitationsCAS_2h0p(walk.d, schd.epsilon, work, *walk.excitedHoles.begin(),
                                           *std::next(walk.excitedHoles.begin()));
  }
  else if (walk.excitation_class == 7) {
    generateAllScreenedExcitationsCAS_2h1p(walk.d, schd.epsilon, work, *walk.excitedOrbs.begin(),
                                           *walk.excitedHoles.begin(), *std::next(walk.excitedHoles.begin()));
  }
  else if (walk.excitation_class == 8) {
    generateAllScreenedExcitationsCAS_2h2p(schd.epsilon, work,
                                           *walk.excitedOrbs.begin(), *std::next(walk.excitedOrbs.begin()),
                                           *walk.excitedHoles.begin(), *std::next(walk.excitedHoles.begin()));
  }

  //if (schd.debug) cout << "phi0  d.energy  " << ham / ovlp << endl;
  //loop over all the screened excitations
  //cout << "m  " << walk.d << endl;
  //cout << "eloc excitations" << endl;
  for (int i=0; i<work.nExcitations; i++) {
    int ex1 = work.excitation1[i], ex2 = work.excitation2[i];
    double tia = work.HijElement[i];

    int I = ex1 / 2 / norbs, A = ex1 - 2 * norbs * I;
    int J = ex2 / 2 / norbs, B = ex2 - 2 * norbs * J;
    double parity = 1.;
    Determinant dcopy = walk.d;
    parity *= dcopy.parity(A/2, I/2, I%2);
    //if (A > I) parity *= -1. * dcopy.parity(A/2, I/2, I%2);
    //else parity *= dcopy.parity(A/2, I/2, I%2);
    dcopy.setocc(I, false);
    dcopy.setocc(A, true);
    if (ex2 != 0) {
      parity *= dcopy.parity(B/2, J/2, J%2);
      //if (B > J) parity *= -1 * dcopy.parity(B/2, J/2, J%2);
      //else parity *= dcopy.parity(B/2, J/2, J%2);
      dcopy.setocc(J, false);
      dcopy.setocc(B, true);
    }

    shortSimpleDet dcopySimple = dcopy.getShortSimpleDet();
    double ovlpcopy = Overlap(dcopySimple);

    ham += tia * ovlpcopy * parity;
    //if (schd.debug) cout << ex1 << "  " << ex2 << "  tia  " << tia << "  ovlpRatio  " << ovlpcopy * parity << endl;

    //work.ovlpRatio[i] = ovlp;
  }
  //if (schd.debug) cout << "ham  " << ham << "  ovlp  " << ovlp << endl << endl;
  ovlp = ham;
}

void SelectedCI::HamAndOvlpLanczos(SimpleWalker &walk,
                       Eigen::VectorXd &lanczosCoeffsSample,
                       double &ovlpSample,
                       workingArray& work,
                       workingArray& moreWork, double &alpha) {

  work.setCounterToZero();
  int norbs = Determinant::norbs;
  double el0 = 0., el1 = 0., ovlp0 = 0., ovlp1 = 0.;
  //ovlp0 = Overlap(walk);
  HamAndOvlp(walk, ovlp0, el0, work);
  std::vector<double> ovlp{0., 0., 0.};
  ovlp[0] = ovlp0;
  ovlp[1] = el0;
  ovlp[2] = ovlp[0] + alpha * ovlp[1];
  if (ovlp[2] == 0) return;

  lanczosCoeffsSample[0] = ovlp[0] * el0 / (ovlp[2] * ovlp[2]);
  lanczosCoeffsSample[1] = ovlp[1] * el0 / (ovlp[2] * ovlp[2]);
  el1 = walk.d.Energy(I1, I2, coreE);

  //workingArray work1;
  //if (schd.debug) cout << "phi1  d.energy  " << el1 << endl;
  //loop over all the screened excitations
  for (int i=0; i<work.nExcitations; i++) {
    double tia = work.HijElement[i];
    int ex1 = work.excitation1[i], ex2 = work.excitation2[i];
    int I = ex1 / 2 / norbs, A = ex1 - 2 * norbs * I;
    int J = ex2 / 2 / norbs, B = ex2 - 2 * norbs * J;
    SimpleWalker walkCopy = walk;
    double parity = 1.;
    Determinant dcopy = walkCopy.d;
    //if (A > I) parity *= -1. * dcopy.parity(A/2, I/2, I%2);
    //else parity *= dcopy.parity(A/2, I/2, I%2);
    parity *= dcopy.parity(A/2, I/2, I%2);
    dcopy.setocc(I, false);
    dcopy.setocc(A, true);
    if (ex2 != 0) {
      //if (B > J) parity *= -1 * dcopy.parity(B/2, J/2, J%2);
      //else parity *= dcopy.parity(B/2, J/2, J%2);
      parity *= dcopy.parity(B/2, J/2, J%2);
    }
    walkCopy.updateWalker(this->bestDeterminant, this->bestDeterminant, work.excitation1[i], work.excitation2[i], false);
    moreWork.setCounterToZero();
    HamAndOvlp(walkCopy, ovlp0, el0, moreWork);
    ovlp1 = el0;
    el1 += parity * tia * ovlp1 / ovlp[1];
    work.ovlpRatio[i] = (ovlp0 + alpha * ovlp1) / ovlp[2];
    //if (schd.debug) cout << work.excitation1[i] << "  " << work.excitation2[i] << "  tia  " << tia << "  ovlpRatio  " << parity * ovlp1 / ovlp[1] << endl;
  }
  //if (schd.debug) cout << endl;
  lanczosCoeffsSample[2] = ovlp[1] * ovlp[1] * el1 / (ovlp[2] * ovlp[2]);
  lanczosCoeffsSample[3] = ovlp[0] * ovlp[0] / (ovlp[2] * ovlp[2]);
  ovlpSample = ovlp[2];
}

//void SelectedCI::getVariables(Eigen::VectorXd &v) {
//  for (int i=0; i<v.rows(); i++)
//    v[i] = coeffs[i];
//}

//long SelectedCI::getNumVariables() {
//  return DetsMap.size();
//}

//void SelectedCI::updateVariables(Eigen::VectorXd &v) {
//  for (int i=0; i<v.rows(); i++)
//    coeffs[i] = v[i];
//}

//void SelectedCI::printVariables() {
//  for (int i=0; i<coeffs.size(); i++)
//    cout << coeffs[i]<<endl;
//}

