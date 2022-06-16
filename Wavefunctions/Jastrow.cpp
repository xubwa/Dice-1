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
#include "Jastrow.h"
#include "Correlator.h"
#include "Determinants.h"
#include <boost/container/static_vector.hpp>
#include <fstream>
#include <iomanip>
#include "input.h"

using namespace Eigen;

Jastrow::Jastrow () {    
  int norbs = Determinant::norbs;
  SpinCorrelator = MatrixXd::Constant(2*norbs, 2*norbs, 1.);
/*
  if (schd.optimizeCps)
    SpinCorrelator += 0.01*MatrixXd::Random(2*norbs, 2*norbs);
*/
  bool readJastrow = false;
  char file[5000];
  sprintf(file, "Jastrow.txt");
  ifstream ofile(file);
  if (ofile)
    readJastrow = true;
  if (readJastrow) {
    for (int i = 0; i < SpinCorrelator.rows(); i++) {
      for (int j = 0; j < SpinCorrelator.rows(); j++){
        ofile >> SpinCorrelator(i, j);
      }
    }
  }
};


double Jastrow::Overlap(const Determinant &d) const
{
  vector<int> closed;
  vector<int> open;
  d.getOpenClosed(open, closed);

  double ovlp = 1.0;
  for (int i=0; i<closed.size(); i++) {
    for (int j=0; j<=i; j++) {
      int I = max(closed[i], closed[j]), J = min(closed[i], closed[j]);

      ovlp *= SpinCorrelator(I, J);
    }
  }
  return ovlp;
}


double Jastrow::OverlapRatio (const Determinant &d1, const Determinant &d2) const {
  return Overlap(d1)/Overlap(d2);
}


double Jastrow::OverlapRatio(int i, int a, const Determinant &dcopy, const Determinant &d) const
{
  return OverlapRatio(dcopy, d);
}

double Jastrow::OverlapRatio(int i, int j, int a, int b, const Determinant &dcopy, const Determinant &d) const
{
  return OverlapRatio(dcopy, d);
}



void Jastrow::OverlapWithGradient(const Determinant& d, 
                              Eigen::VectorBlock<VectorXd>& grad,
                              const double& ovlp) const {
  vector<int> closed;
  vector<int> open;
  d.getOpenClosed(open, closed);

  if (schd.optimizeCps) {
    for (int i=0; i<closed.size(); i++) {
      for (int j=0; j<=i; j++) {
        int I = max(closed[i], closed[j]), J = min(closed[i], closed[j]);
        grad[I*(I+1)/2 + J] += ovlp/SpinCorrelator(I, J);
      }
    }
  }
}

long Jastrow::getNumVariables() const
{
  long spinOrbs = SpinCorrelator.rows();
  return spinOrbs*(spinOrbs+1)/2;
}


void Jastrow::getVariables(Eigen::VectorBlock<VectorXd> &v) const
{
  int numVars = 0;
  for (int i=0; i<SpinCorrelator.rows(); i++)
    for (int j=0; j<=i; j++) {
      v[numVars] = SpinCorrelator(i,j);
      numVars++;
    }
}

void Jastrow::updateVariables(const Eigen::VectorBlock<VectorXd> &v)
{
  int numVars = 0;
  for (int i=0; i<SpinCorrelator.rows(); i++)
    for (int j=0; j<=i; j++) {
      SpinCorrelator(i,j) = v[numVars];
      numVars++;
    }

}

void Jastrow::addNoise() {
 SpinCorrelator += 0.01 * MatrixXd::Random(2*Determinant::norbs, 2*Determinant::norbs);
}

void Jastrow::printVariables() const
{
  cout << "Jastrow"<< endl;
  //for (int i=0; i<SpinCorrelator.rows(); i++)
  //  for (int j=0; j<=i; j++)
  //    cout << SpinCorrelator(i,j);
  cout << SpinCorrelator << endl << endl;
}

void Jastrow::printVariablesToFile() const
{
  string jastrowFileName = "JASTROW";
  ofstream out_jastrow;
  out_jastrow.open(jastrowFileName);

  out_jastrow.precision(12);
  out_jastrow << scientific;
  for (int i=0; i<SpinCorrelator.rows(); i++) {
    for (int j=0; j<=i; j++) {
      out_jastrow << " " << setw(4) << i << "   " << setw(4) << j << "   " << SpinCorrelator(i,j) << endl;
    }
  }

  out_jastrow.close();
}
