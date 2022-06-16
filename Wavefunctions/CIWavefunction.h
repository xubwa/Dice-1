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

#ifndef CIWavefunction_HEADER_H
#define CIWavefunction_HEADER_H
#include <vector>
#include <set>
#include "Determinants.h"
#include "workingArray.h"
#include "excitationOperators.h"
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/array.hpp>
#include <utility>

using namespace std;

class oneInt;
class twoInt;
class twoIntHeatBathSHM;



/**
 * This is the wavefunction, that extends a given wavefunction by doing
 * a CI expansion on it
 * |Psi> = \sum_i d_i O_i ||phi>
 * where d_i are the coefficients and O_i are the list of operators
 * these operators O_i can be of type Operators or SpinFreeOperators
 */
template <typename Wfn, typename Walker, typename OpType>
  class CIWavefunction
{
 private:
  friend class boost::serialization::access;
  template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
      ar  & wave
	& oplist
	& ciCoeffs;
    //& excitedOvlps;
    }

  
 public:
  Wfn wave;
  std::vector<OpType> oplist;
  std::vector<double> ciCoeffs;
  //Eigen::VectorXd excitedOvlps; //stores overlaps: <n|oplist[i]|phi0> for the current walker |n>, filled during Overlap call

  CIWavefunction() {
    wave.readWave();
    oplist.resize(1);
    ciCoeffs.resize(1, 1.0);
  }

 CIWavefunction(Wfn &w1, std::vector<OpType> &pop) : wave(w1), oplist(pop)
  {
    ciCoeffs.resize(oplist.size(), 0.0);
    ciCoeffs[0] = 1.0;
  }


  typename Wfn::ReferenceType& getRef() { return wave.getRef(); }
  typename Wfn::CorrType& getCorr() { return wave.getCorr(); }

  void appendSinglesToOpList(double screen = 1.0)
  {
    OpType::populateSinglesToOpList(oplist, ciCoeffs, screen);
    ciCoeffs.resize(oplist.size(), 0.0);
    //excitedOvlps.resize(oplist.size(), 0.0);
  }

  void appendScreenedDoublesToOpList(double screen)
  {
    OpType::populateScreenedDoublesToOpList(oplist, ciCoeffs, screen);
    //OpType::populateDoublesToOpList(oplist, ciCoeffs);
    ciCoeffs.clear(); ciCoeffs.resize(oplist.size(), 0.0); ciCoeffs[0] = 1.;
    //excitedOvlps = Eigen::VectorXd::Zero(oplist.size());
    if (schd.ciCeption) {
      uniform_real_distribution<double> dist(0.005,0.006);
      for (int i = 1; i<ciCoeffs.size(); i++) ciCoeffs[i] = dist(generator);
    }
  }

  void getVariables(VectorXd& vars) {
    if (vars.rows() != getNumVariables())
      vars = VectorXd::Zero(getNumVariables());
    for (int i=0; i<ciCoeffs.size(); i++)
      vars[i] = ciCoeffs[i];
  }

  void printVariables() {
    for (int i=0;i<oplist.size(); i++)
      cout << oplist[i]<<"  "<<ciCoeffs[i]<<endl;
  }

  void updateVariables(VectorXd& vars) {
    for (int i=0; i<vars.rows(); i++)
      ciCoeffs[i] = vars[i];
  }

  long getNumVariables() {
    return ciCoeffs.size();
  }

  double calculateOverlapRatioWithUnderlyingWave(Walker &walk, Determinant &dcopy)
  {
    double ovlpdetcopy;
    int excitationDistance = dcopy.ExcitationDistance(walk.d);

    if (excitationDistance == 0)
      {
	ovlpdetcopy = 1.0;
      }
    else if (excitationDistance == 1)
      {
	int I, A;
	getDifferenceInOccupation(walk.d, dcopy, I, A);
	ovlpdetcopy = wave.getOverlapFactor(I, A, walk, false);
      }
    else if (excitationDistance == 2)
      {
	int I, J, A, B;
	getDifferenceInOccupation(walk.d, dcopy, I, J, A, B);
	bool doparity = false;

	//cout << I<<"  "<<J<<"  "<<A<<"  "<<B<<endl;
	ovlpdetcopy = wave.getOverlapFactor(I, J, A, B, walk, doparity);
      }
    else
      {
	cout << "higher than triple excitation not yet supported." << endl;
	exit(0);
      }
    return ovlpdetcopy;
  }


  double getOverlapFactor(int I, int J, int A, int B, Walker& walk, bool doparity=false) {
    int norbs = Determinant::norbs;
    if (J == 0 && B == 0) {
      Walker walkcopy = walk;
      walkcopy.exciteWalker(wave.getRef(), wave.getCorr(), I*2*norbs+A, 0, norbs);
      return Overlap(walkcopy)/Overlap(walk);
    }
    else {
      Walker walkcopy = walk;
      walkcopy.exciteWalker(wave.getRef(), wave.getCorr(), I*2*norbs+A, J*2*norbs+B, norbs);
      return Overlap(walkcopy)/Overlap(walk);
    }
  }
  
  double Overlap(Walker& walk) {
    double totalovlp = 0.0;
    double ovlp0 = wave.Overlap(walk); 
    //excitedOvlps.setZero();
    //excitedOvlps[0] = ovlp0;
    totalovlp += ciCoeffs[0] * ovlp0;

	Determinant dcopy = walk.d;
    int opSize = oplist.size();
    //int nops = 1;//change if using spin-free operators
    for (int i = 1; i < opSize; i++) {
	  for (int j = 0; j < oplist[i].nops; j++) {
	  //for (int j=0; j<nops; j++) {
        dcopy = walk.d;
	    bool valid = 0;
	    double ovlpdetcopy = 0.;
        if (schd.ciCeption) { //uncomment to work with non-sci waves
          //for (int k = 0; k < op.n; k++) if (op.cre[k] >= 2*schd.nciAct) valid = 1;
          //if (valid) continue;
          valid = oplist[i].apply(dcopy, walk.excitedOrbs);
          if (valid) {
            Walker walkcopy(wave.getCorr(), wave.getRef(), dcopy);
            ovlpdetcopy = wave.Overlap(walkcopy); 
            //excitedOvlps[i] = ovlpdetcopy;
	        totalovlp += ciCoeffs[i] * ovlpdetcopy;
          }
        }
        else { 
          valid = oplist[i].apply(dcopy, j);
          if (valid) {
            ovlpdetcopy = calculateOverlapRatioWithUnderlyingWave(walk, dcopy);
	        totalovlp += ciCoeffs[i] * ovlpdetcopy * ovlp0;
          }
        }
	  }
    }
    return totalovlp;
  }

  double OverlapWithGradient(Walker &walk,
			     double &factor,
			     Eigen::VectorXd &grad)
  {
    VectorXd gradcopy = grad;
    gradcopy.setZero();
    
    double ovlp0 = Overlap(walk);
    //double ovlp0 = excitedOvlps[0];
    if (ovlp0 == 0.) return 0.;

    for (int i = 0; i < oplist.size(); i++) {
      for (int j=0; j<oplist[i].nops; j++) {
	  Determinant dcopy = walk.d;
	  bool valid = 0;
	  double ovlpdetcopy = 0.;
      if (schd.ciCeption) {
        valid = oplist[i].apply(dcopy, walk.excitedOrbs);
        if (valid) {
          Walker walkcopy(wave.getCorr(), wave.getRef(), dcopy);
          ovlpdetcopy = wave.Overlap(walkcopy) / ovlp0; 
        }
        //ovlpdetcopy = excitedOvlps[i] / ovlp0; 
      }
      else { 
        valid = oplist[i].apply(dcopy, j);
        if (valid) {
          ovlpdetcopy = calculateOverlapRatioWithUnderlyingWave(walk, dcopy);
        }
      }
	  gradcopy[i] += ovlpdetcopy;
      }
    }

    double totalOvlp = 0.0;
    if (schd.ciCeption) totalOvlp = 1.;
    else {
      for (int i=0; i<grad.rows(); i++) {
        totalOvlp += ciCoeffs[i] * gradcopy[i];
      }
    }

    for (int i=0; i<grad.rows(); i++) {
      grad[i] += gradcopy[i]/totalOvlp;
    }
    return totalOvlp;
  }


  void HamAndOvlp(Walker &walk,
                  double &ovlp, double &ham, 
                  workingArray& work, bool fillExcitations=true)
  {
    work.setCounterToZero();
    int norbs = Determinant::norbs;

    ovlp = Overlap(walk);

    if (ovlp == 0.) return;
    ham = walk.d.Energy(I1, I2, coreE); 


    generateAllScreenedSingleExcitation(walk.d, schd.epsilon, schd.screen,
                                        work, !schd.ciCeption);  
    generateAllScreenedDoubleExcitation(walk.d, schd.epsilon, schd.screen,
                                        work, !schd.ciCeption);  

    //loop over all the screened excitations
    for (int i=0; i<work.nExcitations; i++) {
      int ex1 = work.excitation1[i], ex2 = work.excitation2[i];
      double tia = work.HijElement[i];
    
      int I = ex1 / 2 / norbs, A = ex1 - 2 * norbs * I;
      int J = ex2 / 2 / norbs, B = ex2 - 2 * norbs * J;

      Walker walkcopy = walk;
      if (ex2 == 0) walkcopy.updateWalker(wave.getRef(), wave.getCorr(), ex1, ex2, true);
      else walkcopy.updateWalker(wave.getRef(), wave.getCorr(), ex1, ex2, false);
      if (schd.ciCeption && (walkcopy.excitedOrbs.size() > 2)) continue;
      double parity = 1.0;
      if (schd.ciCeption) {
        Determinant dcopy = walk.d;
        if (A > I) parity *= -1. * dcopy.parity(A/2, I/2, I%2);
        else parity *= dcopy.parity(A/2, I/2, I%2);
        dcopy.setocc(I, false); dcopy.setocc(A, true);
        if (ex2 != 0) {
          if (B > J) parity *= -1 * dcopy.parity(B/2, J/2, J%2);
          else parity *= dcopy.parity(B/2, J/2, J%2);
        }
      }
      double ovlpdetcopy = Overlap(walkcopy);
      if (schd.debug) {
        cout << "walkCopy    " << walkcopy << endl;
        cout << "det energy   " << ham << endl; 
        cout << ex1 << "  " << ex2 << "  tia  " << tia << "  ovlpRatio  " << parity * ovlpdetcopy / ovlp << endl;
      }

      ham += tia * parity * ovlpdetcopy / ovlp;
      work.ovlpRatio[i] = ovlpdetcopy/ovlp;
    }
  }

  void initWalker(Walker& walk)
  {
    wave.initWalker(walk);
  }
  
  void initWalker(Walker& walk, Determinant& d)
  {
    wave.initWalker(walk, d);
  }

  string getfileName() const {
    return "ci"+wave.getfileName();
  }

  void writeWave()
  {
    if (commrank == 0)
      {
	char file[5000];
        sprintf(file, (getfileName()+".bkp").c_str() );
	std::ofstream outfs(file, std::ios::binary);
	boost::archive::binary_oarchive save(outfs);
	save << *this;
	outfs.close();
      }
  }

  void readWave()
  {
    if (commrank == 0)
      {
	char file[5000];
        sprintf(file, (getfileName()+".bkp").c_str() );
	std::ifstream infs(file, std::ios::binary);
	boost::archive::binary_iarchive load(infs);
	load >> *this;
	infs.close();
      }
#ifndef SERIAL
    boost::mpi::communicator world;
    boost::mpi::broadcast(world, *this, 0);
#endif
  }

  // not used
  //template<typename Walker>
  //bool checkWalkerExcitationClass(Walker &walk) {
  //  return true;
  //}

};

/*
template <typename Wfn, typename Walker>
  class Lanczos : public CIWavefunction<Wfn, Walker, Operator>
{
 private:
  friend class boost::serialization::access;
  template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
      ar  & boost::serialization::base_object<CIWavefunction<Wfn, Walker, Operator>>(*this)
	& alpha;
    }

 public:
  double alpha;

  Lanczos()
  {
    CIWavefunction<Wfn, Walker, Operator>();
    alpha = 0.;
  }

  Lanczos(Wfn &w1, std::vector<Operator> &pop, double alpha0) : alpha(alpha0)
  {
    CIWavefunction<Wfn, Walker, Operator>(w1, pop);
  }

  typename Wfn::ReferenceType& getRef() { return wave.getRef(); }
  typename Wfn::CorrType& getCPS() { return wave.getCPS(); }

  void appendSinglesToOpList(double screen = 0.0)
  {
    Operator::populateSinglesToOpList(this->oplist, this->ciCoeffs);
    //ciCoeffs.resize(oplist.size(), 0.0);
  }

  void appendScreenedDoublesToOpList(double screen)
  {
    Operator::populateScreenedDoublesToOpList(this->oplist, this->ciCoeffs, screen);
    //ciCoeffs.resize(oplist.size(), 0.0);
  }

  void initWalker(Walker& walk) {
    this->wave.initWalker(walk);
  }  

  void initWalker(Walker& walk, Determinant& d) {
    this->wave.initWalker(walk, d);
  }  

  void getVariables(VectorXd& vars) {
    if (vars.rows() != getNumVariables())
      {
	vars = VectorXd::Zero(getNumVariables());
      }
      vars[0] = alpha;
  }

  void printVariables() {
    cout << "alpha  " << alpha << endl;
    //for (int i=0; i<this->oplist.size(); i++)
    //  cout << this->oplist[i] << "  " << this->ciCoeffs[i] << endl;
  }

  void updateVariables(VectorXd& vars) {
    alpha = vars[0];
  }

  long getNumVariables() {
    return 1;
  }
  
  double Overlap(Walker& walk) {
    double totalovlp = 0.0;
    double ovlp0 = this->wave.Overlap(walk);
    totalovlp += ovlp0;

    for (int i=1; i<this->oplist.size(); i++)
      {
	for (int j=0; j<this->oplist[i].nops; j++) {
	  Determinant dcopy = walk.d;
	  bool valid =this-> oplist[i].apply(dcopy, j);

	  if (valid) {
	    double ovlpdetcopy = calculateOverlapRatioWithUnderlyingWave(walk, dcopy);
	    totalovlp += alpha * this->ciCoeffs[i] * ovlpdetcopy * ovlp0;
	  }
	}
      }
    return totalovlp;
  }

  double OverlapWithGradient(Walker &walk,
			     double &factor,
			     Eigen::VectorXd &grad)
  {

    double num = 0.; 
    for (int i=1; i<this->oplist.size(); i++)
      {
	for (int j=0; j<this->oplist[i].nops; j++) {
	  Determinant dcopy = walk.d;
	  
	  bool valid = this->oplist[i].apply(dcopy, j);

	  if (valid) {
	    double ovlpdetcopy = calculateOverlapRatioWithUnderlyingWave(walk, dcopy);
	    num += this->ciCoeffs[i] * ovlpdetcopy;
	  }
	}
      }

    grad[0] += num/(alpha * num + 1);
    return (alpha * num + 1);
  }

  void HamAndOvlp(Walker &walk,
		  double &ovlp, double &ham,
		  workingArray& work, bool fillExcitations = true)
  {
    work.setCounterToZero();

    double TINY = schd.screen;
    double THRESH = schd.epsilon;

    Determinant &d = walk.d;

    int norbs = Determinant::norbs;
    vector<int> closed;
    vector<int> open;
    d.getOpenClosed(open, closed);

    //noexcitation
    {
      double E0 = d.Energy(I1, I2, coreE);
      ovlp = Overlap(walk);
      ham = E0;
    }

    //Single alpha-beta excitation
    {
      double time = getTime();
      for (int i = 0; i < closed.size(); i++)
	{
	  for (int a = 0; a < open.size(); a++)
	    {
	      if (closed[i] % 2 == open[a] % 2 && abs(I2hb.Singles(closed[i], open[a])) > THRESH)
		{
		  prof.SinglesCount++;

		  int I = closed[i] / 2, A = open[a] / 2;
		  double tia = 0;
		  Determinant dcopy = d;
		  bool Alpha = closed[i] % 2 == 0 ? true : false;

		  bool doparity = true;
		  if (schd.Hamiltonian == HUBBARD)
		    {
		      tia = I1(2 * A, 2 * I);
		      double sgn = 1.0;
		      if (Alpha)
                sgn *= d.parityA(A, I);
		      else
                sgn *= d.parityB(A, I);
		      tia *= sgn;
		    }
		  else
		    {
		      tia = I1(2 * A, 2 * I);
		      int X = max(I, A), Y = min(I, A);
		      int pairIndex = X * (X + 1) / 2 + Y;
		      size_t start = I2hb.startingIndicesSingleIntegrals[pairIndex];
		      size_t end = I2hb.startingIndicesSingleIntegrals[pairIndex + 1];
		      float *integrals = I2hb.singleIntegrals;
		      short *orbIndices = I2hb.singleIntegralsPairs;
		      for (size_t index = start; index < end; index++)
			{
			  if (fabs(integrals[index]) < TINY)
			    break;
			  int j = orbIndices[2 * index];
			  if (closed[i] % 2 == 1 && j % 2 == 1)
			    j--;
			  else if (closed[i] % 2 == 1 && j % 2 == 0)
			    j++;

			  if (d.getocc(j))
			    {
			      tia += integrals[index];
			    }
			  //cout << tia<<"  "<<a<<"  "<<integrals[index]<<endl;
			}
		      double sgn = 1.0;
		      if (Alpha)
                sgn *= d.parityA(A, I);
		      else
                sgn *= d.parityB(A, I);
		      tia *= sgn;
		    }

		  double localham = 0.0;
		  if (abs(tia) > THRESH)
		    {
		      Walker walkcopy = walk;
		      walkcopy.exciteWalker(this->wave.getRef(), this->wave.getCPS(), closed[i]*2*norbs+open[a], 0, norbs);
		      double ovlpdetcopy = Overlap(walkcopy);
		      ham += ovlpdetcopy * tia / ovlp;

		      if (fillExcitations)
                work.appendValue(ovlpdetcopy/ovlp, closed[i] * 2 * norbs + open[a], 0, tia);
		    }
		}
	    }
	}
      prof.SinglesTime += getTime() - time;
    }

    if (schd.Hamiltonian == HUBBARD)
      return;

    //Double excitation
    {
      double time = getTime();

      int nclosed = closed.size();
      for (int ij = 0; ij < nclosed * nclosed; ij++)
	{
	  int i = ij / nclosed, j = ij % nclosed;
	  if (i <= j)
	    continue;
	  int I = closed[i] / 2, J = closed[j] / 2;
	  int X = max(I, J), Y = min(I, J);

	  int pairIndex = X * (X + 1) / 2 + Y;
	  size_t start = closed[i] % 2 == closed[j] % 2 ? I2hb.startingIndicesSameSpin[pairIndex] : I2hb.startingIndicesOppositeSpin[pairIndex];
	  size_t end = closed[i] % 2 == closed[j] % 2 ? I2hb.startingIndicesSameSpin[pairIndex + 1] : I2hb.startingIndicesOppositeSpin[pairIndex + 1];
	  float *integrals = closed[i] % 2 == closed[j] % 2 ? I2hb.sameSpinIntegrals : I2hb.oppositeSpinIntegrals;
	  short *orbIndices = closed[i] % 2 == closed[j] % 2 ? I2hb.sameSpinPairs : I2hb.oppositeSpinPairs;

	  // for all HCI integrals
	  for (size_t index = start; index < end; index++)
	    {
	      // if we are going below the criterion, break
	      if (fabs(integrals[index]) < THRESH)
		break;

	      // otherwise: generate the determinant corresponding to the current excitation
	      int a = 2 * orbIndices[2 * index] + closed[i] % 2, b = 2 * orbIndices[2 * index + 1] + closed[j] % 2;
	      if (!(d.getocc(a) || d.getocc(b)))
		{
		  int A = a / 2, B = b / 2;
		  double tiajb = integrals[index];

		  Walker walkcopy = walk;
		  walkcopy.exciteWalker(this->wave.getRef(), this->wave.getCPS(), closed[i] * 2 * norbs + a, closed[j] * 2 * norbs + b, norbs);
		  double ovlpdetcopy = Overlap(walkcopy);

		  double parity = 1.0;
		  if (closed[i] % 2 == closed[j] % 2 && closed[i] % 2 == 0)
		    parity = walk.d.parityAA(I, J, A, B); 
		  else if (closed[i] % 2 == closed[j] % 2 && closed[i] % 2 == 1)
		    parity = walk.d.parityBB(I, J, A, B); 
		  else if (closed[i] % 2 != closed[j] % 2 && closed[i] % 2 == 0)
		    parity = walk.d.parityA(A, I) * walk.d.parityB(B, J);
		  else
		    parity = walk.d.parityB(A, I) * walk.d.parityA(B, J);
            
		  ham += ovlpdetcopy * tiajb * parity / ovlp;

		  if (fillExcitations)
		    work.appendValue(ovlpdetcopy/ovlp, closed[i] * 2 * norbs + a,
				     closed[j] * 2 * norbs + b , tiajb);
		}
	    }
	}
      prof.DoubleTime += getTime() - time;
    }
  }


  void writeWave()
  {
    if (commrank == 0)
      {
	char file[5000];
	//sprintf (file, "wave.bkp" , schd.prefix[0].c_str() );
	sprintf(file, "lanczoswave.bkp");
	std::ofstream outfs(file, std::ios::binary);
	boost::archive::binary_oarchive save(outfs);
	save << *this;
	outfs.close();
      }
  }

  void readWave()
  {
    if (commrank == 0)
      {
	char file[5000];
	//sprintf (file, "wave.bkp" , schd.prefix[0].c_str() );
	sprintf(file, "lanczoswave.bkp");
	std::ifstream infs(file, std::ios::binary);
	boost::archive::binary_iarchive load(infs);
	load >> *this;
	infs.close();
      }
#ifndef SERIAL
    boost::mpi::communicator world;
    boost::mpi::broadcast(world, *this, 0);
#endif
  }
};
*/
#endif
