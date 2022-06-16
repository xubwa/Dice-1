#include "cdfci.h"
#include "input.h"
#include "math.h"
#include "SHCIbasics.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <map>
#include <tuple>
#include <vector>
#include "omp.h"
#include "Determinants.h"
#include "SHCIgetdeterminants.h"
#include "SHCISortMpiUtils.h"
#include "SHCItime.h"
#include "input.h"
#include "integral.h"
#include "math.h"
#include "communicate.h"
#include <boost/format.hpp>
#include <Eigen/Core>
#include <algorithm>
#include <iomanip>

using namespace std;
using namespace Eigen;
using namespace boost;
using StitchDEH = SHCISortMpiUtils::StitchDEH;
using cdfci::value_type;
typedef unordered_map<Determinant, array<CItype, 2>> hash_det;

void cdfci::getDeterminantsVariational(
        Determinant& d, double epsilon, CItype ci1, CItype ci2,
        oneInt& int1, twoInt& int2, twoIntHeatBathSHM& I2hb,
        vector<int>& irreps, double coreE, double E0,
        cdfci::DetToIndex& det_to_index,
        schedule& schd, int Nmc, int nelec) {
//-----------------------------------------------------------------------------
    /*!
    Make the int represenation of open and closed orbitals of determinant
    this helps to speed up the energy calculation

    :Inputs:

        Determinant& d:
            The reference |D_i>
        double epsilon:
            The criterion for chosing new determinants (understood as epsilon/c_i)
        CItype ci1:
            The reference CI coefficient c_i
        CItype ci2:
            The reference CI coefficient c_i
        oneInt& int1:
            One-electron tensor of the Hamiltonian
        twoInt& int2:
            Two-electron tensor of the Hamiltonian
        twoIntHeatBathSHM& I2hb:
            The sorted two-electron integrals to choose the bi-excited determinants
        vector<int>& irreps:
            Irrep of the orbitals
        double coreE:
            The core energy
        double E0:
            The current variational energy
        std::vector<Determinant>& dets:
            The determinants' determinant
        schedule& schd:
            The schedule
        int Nmc:
            BM_description
        int nelec:
            Number of electrons
    */
//-----------------------------------------------------------------------------

  // initialize variables
  int norbs   = d.norbs;
  int nclosed = nelec;
  int nopen   = norbs-nclosed;
  vector<int> closed(nelec,0);
  vector<int> open(norbs-nelec,0);
  d.getOpenClosed(open, closed);
  int unpairedElecs = schd.enforceSeniority ?  d.numUnpairedElectrons() : 0;

  initiateRestrictions(schd, closed);
  for (int ia=0; ia<nopen*nclosed; ia++){
    int i=ia/nopen, a=ia%nopen;

    CItype integral = I2hb.Singles(open[a], closed[i]);//Hij_1Excite(open[a],closed[i],int1,int2, &closed[0], nclosed);
    //std::cout << "integral : " << integral << " epsilon : " << epsilon << std::endl;
    if (fabs(integral) > epsilon)
      if (closed[i]%2 == open[a]%2)
        integral = Hij_1Excite(open[a],closed[i],int1,int2, &closed[0], nclosed);

    //if (closed[i]%2 != open[a]%2) {
    //  integral = int1(open[a], closed[i])*schd.socmultiplier;
    //}
    //Have question about the SOC stuff

    // generate determinant if integral is above the criterion
    if (std::abs(integral) > epsilon ) {
      Determinant di = d;
      di.setocc(open[a], true); di.setocc(closed[i],false);
      //std::cout << "single excitation: " << di << std::endl;
        if(det_to_index.find(di) == det_to_index.end()) {
          int last_index = det_to_index.size();
          det_to_index[di] = last_index;
        }
      //if (Determinant::Trev != 0) di.makeStandard();
    }
  } // ia

  // bi-excitated determinants
  if (std::abs(int2.maxEntry) < epsilon) return;
  // for all pairs of closed
  for (int ij=0; ij<nclosed*nclosed; ij++) {
    int i=ij/nclosed, j = ij%nclosed;
    if (i<=j) continue;
    int I = closed[i]/2, J = closed[j]/2;
    int X = max(I, J), Y = min(I, J);

    if (closed[i]/2 < schd.ncore || closed[j]/2 < schd.ncore) continue;

    int pairIndex = X*(X+1)/2+Y;
    size_t start = closed[i]%2==closed[j]%2 ? I2hb.startingIndicesSameSpin[pairIndex]   : I2hb.startingIndicesOppositeSpin[pairIndex];
    size_t end   = closed[i]%2==closed[j]%2 ? I2hb.startingIndicesSameSpin[pairIndex+1] : I2hb.startingIndicesOppositeSpin[pairIndex+1];
    float* integrals  = closed[i]%2==closed[j]%2 ?  I2hb.sameSpinIntegrals : I2hb.oppositeSpinIntegrals;
    short* orbIndices = closed[i]%2==closed[j]%2 ?  I2hb.sameSpinPairs     : I2hb.oppositeSpinPairs;

    // for all HCI integrals
    //std::cout << "double excitation" << std::endl;
    for (size_t index=start; index<end; index++) {
      // if we are going below the criterion, break
      if (fabs(integrals[index]) < epsilon) break;

      // otherwise: generate the determinant corresponding to the current excitation
      int a = 2* orbIndices[2*index] + closed[i]%2, b= 2*orbIndices[2*index+1]+closed[j]%2;
      //double E = EnergyAfterExcitation(closed, nclosed, int1, int2, coreE, i, a, j, b, Energyd);
      //if (abs(integrals[index]/(E0-Energyd)) <epsilon) continue;
      if (a/2 >= schd.ncore+schd.nact || b/2 >= schd.ncore+schd.nact) continue;
      if (! satisfiesRestrictions(schd, closed[i], closed[j], a, b)) continue;
      if (!(d.getocc(a) || d.getocc(b))) {
        Determinant di = d;
        di.setocc(a, true); di.setocc(b, true);di.setocc(closed[i],false); di.setocc(closed[j], false);
        if(det_to_index.find(di) == det_to_index.end()) {
          int last_index = det_to_index.size();
          det_to_index[di] = last_index;
        }
        //if (Determinant::Trev != 0) di.makeStandard();
      }
    } // heatbath integrals
  } // ij
  return;
} // end SHCIgetdeterminants::getDeterminantsVariational

vector<value_type> cdfci::getSubDets(value_type& d, hash_det& wfn, int nelec, bool sample) {
  auto det = d.first;
  int norbs = det.norbs;
  int nclosed = nelec;
  int nopen = norbs-nclosed;
  vector<int> closed(nelec, 0);
  vector<int> open(nopen, 0);
  det.getOpenClosed(open, closed);
    
  vector<value_type> result;
  result.push_back(d);
  //get all existing single excitationsgetDeterminantsVariational
  for (int ia=0; ia<nopen*nclosed; ia++) {
    int i=ia/nopen, a=ia%nopen;
    if (closed[i]%2 == open[a]%2) {
      Determinant di = det;
      di.setocc(open[a], true);
      di.setocc(closed[i], false);
      if (wfn.find(di) != wfn.end()) {
        value_type val = std::make_pair(di, wfn[di]);
        result.push_back(val);
      }
      else {
        if (sample == true) {
          value_type val = std::make_pair(di, std::array<double, 2> {0.0, 0.0});
          result.push_back(val); 
        }
      }
    }
    else 
      continue;
  }
  //get all existing double excitations
  for(int ij=0; ij<nclosed*nclosed; ij++) {
    int i=ij/nclosed, j=ij%nclosed;
    if(i<=j) continue;
    int I = closed[i], J = closed[j];
    for (int kl=0;kl<nopen*nopen;kl++) {
      int k=kl/nopen;
      int l=kl%nopen;
      if(k<=l) continue;
      int K=open[k], L=open[l];
      int a = max(K,L), b = min(K,L);
      if (a%2+b%2-I%2-J%2 != 0) {
        continue;
      }
      Determinant di = det;
      di.setocc(a, true);
      di.setocc(b, true);
      di.setocc(I, false);
      di.setocc(J, false);
      if (wfn.find(di)!=wfn.end()){
        value_type val = std::make_pair(di, wfn[di]);
        result.push_back(val);
      }
      else {
        if (sample == true) {
          value_type val = std::make_pair(di, std::array<double, 2> {0.0, 0.0});
          result.push_back(val); 
        }
      }
    }
  }
  return result;
}

value_type cdfci::CoordinatePickGcdGrad(vector<value_type> sub, double norm) {
  double max_abs_grad = 0;
  auto result = sub.begin();
  for(auto iter=sub.begin()+1; iter!=sub.end(); iter++) {
    auto x = iter->second[0];
    auto z = iter->second[1];
    double abs_grad = fabs(x*norm+z);
    if (abs_grad >= max_abs_grad) {
      max_abs_grad = abs_grad;
      result = iter;
    }
  }
  return std::make_pair(result->first, result->second);
}

double cdfci::CoordinateUpdate(value_type& det_picked, hash_det & wfn, pair<double,double>& ene, vector<double> E0, oneInt& I1, twoInt& I2, double& coreE) {
  // coreE doesn't matter.
  double dx = 0.0;
  auto det = det_picked.first;
  auto x = det_picked.second[0];
  auto z = det_picked.second[1];
  // need to store a wavefunction norm here.
  double xx = ene.second;
  //std::cout << x << " z:" << z << " xx:" << xx << std::endl; 
  size_t orbDiff;

  auto dA = det.Energy(I1, I2, coreE);
  dA=-dA;
  // Line Search, cced from original cdfci code.
  auto p1 = xx - x * x - dA;
  auto q = z + dA * x;  //
  auto p3 = p1/3;
  auto q2 = q/2;
  auto d = p3 * p3 * p3 + q2 * q2;
  double rt = 0;
  //std::cout << "p1: " << p1 << " q: " << q << std::endl;
  const double pi = atan(1.0) * 4;

  if (d >= 0) {
    auto qrtd = sqrt(d);
    rt = cbrt(-q2 + qrtd) + cbrt(-q2 - qrtd);
  }
  else {
    auto qrtd = sqrt(-d);
    if (q2 >= 0) {
      rt = 2 * sqrt(-p3) * cos((atan2(-qrtd, -q2) - 2*pi)/3.0);      
    }
    else {
      rt = 2 * sqrt(-p3) * cos(atan2(qrtd, -q2)/3.0);
    }
  }
  dx = rt - x;
// Newton iteration to improve accuracy
  auto dxn = dx - (dx*(dx*(dx + 3*x) + (3*x*x + p1)) + p1*x + q + x*x*x)
             / (dx*(3*dx + 6*x) + 3*x*x + p1);

  const double depsilon = 1e-12;
  const int max_iter = 10;
  int iter = 0;
  while (fabs((dxn - dx)/dx) > depsilon && iter < max_iter)
  {
      dx = dxn;
      dxn = dx - (dx*(dx*(dx + 3*x) + (3*x*x + p1)) + p1*x + q + x*x*x)
          / (dx*(3*dx + 6*x) + 3*x*x + p1);
      ++iter;
  }
  return dx;
}

void cdfci::civectorUpdate(vector<value_type>& column, hash_det& wfn, double dx, pair<double, double>& ene, oneInt& I1, twoInt& I2, double& coreE, double thresh, bool sample) {
  auto deti = column[0].first;
  //std::cout << "selected det: " << deti << std::endl;
  size_t orbDiff;
  //std::cout << ene.first << " " << ene.second << std::endl;
  for (auto entry = column.begin(); entry!=column.end(); entry++) {
    auto detj = entry->first;
    double hij;
    auto iter = wfn.find(detj);
    if (detj == deti) {
      hij = deti.Energy(I1, I2, coreE);
      ene.first += hij * dx * dx+2*dx*hij*wfn[detj][0];
      ene.second += dx*dx + 2* wfn[detj][0]*dx;
      //std::cout << wfn[detj][1] << " " << wfn[detj][0] << " " << ene.first << std::endl;
      wfn[detj][0] += dx;
      wfn[detj][1] += hij*dx;
    }
    else {
      hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
      double dz = dx*hij;
      if (sample == false) {
        ene.first += 2*iter->second[0]*dz;
        wfn[detj][1] += dz;
      }
      else {
        if (iter != wfn.end()) {
          ene.first += 2*iter->second[0]*dz;
          wfn[detj][1] += dz;
        }
        else {
          if (std::fabs(dz) > thresh) {
            wfn[detj]={0.0, dz};
            entry->second[1]=dz;
          }
        }
      }
    }
    
    //std::cout << "det:" << detj << " civec:" << wfn[detj][0] << " hx:" << wfn[detj][1] << std::endl;
  }
    
  return; 
}

cdfci::hash_det cdfci::precondition(pair<double, double>& ene, vector<Determinant>& dets, vector<MatrixXx>& ci, vector<double> energy, oneInt& I1, twoInt& I2, double& coreE, double thresh, bool sample) {
  auto norm = std::sqrt(std::abs(energy[0]-coreE));
  hash_det wfn;
  ene = std::make_pair(0.0, 0.0);
  const int nelec = dets[0].Noccupied();
  double coreEbkp = coreE;
  coreE = 0.0;
  std::array<double, 2> zeros{0.0, 0.0};
  auto dets_size = dets.size();
  for (int i = 0; i < dets_size; i++) {
    wfn[dets[i]] = zeros;
  }
  for (int i = 0; i < dets_size; i++) {
    auto dx = ci[0](i,0)*norm;
    auto thisDet = std::make_pair(dets[i], wfn[dets[i]]);
    auto column = cdfci::getSubDets(thisDet, wfn, nelec, sample);
    cdfci::civectorUpdate(column, wfn, dx, ene, I1, I2, coreE, thresh, sample);
  }
  coreE = coreEbkp;
  return wfn;
}

set<Determinant> cdfci::sampleExtraEntry(hash_det& wfn, int nelec) {
  set<Determinant> result;
  for(auto det : wfn) {
    value_type std_det = std::make_pair(det.first, det.second);
    vector<value_type> column = cdfci::getSubDets(std_det, wfn, nelec, true);
    for (auto entry : column) {
      if (std::fabs(entry.second[0]) < 1e-100 && std::fabs(entry.second[1]) < 1e-100) {
        result.insert(entry.first);
      }
    }
    if (result.size() > 10000) {
      break;
    }
  }
  return result;
}
 
void cdfci::cdfciSolver(hash_det& wfn, Determinant& hf, schedule& schd, pair<double, double>& ene, oneInt& I1, twoInt& I2, twoIntHeatBathSHM& I2HB, vector<int>& irrep, double& coreE, vector<double>& E0, int nelec, double thresh, bool sample) {
    // first to initialize hx for new determinants
    auto coreEbkp = coreE;
    coreE = 0.0;
    auto startofCalc = getTime();
    value_type thisDet =  std::make_pair(hf, wfn[hf]);
    double prev_ene;
    if (fabs(ene.first) > 1e-10)
      prev_ene = ene.first/ene.second;
    else
      prev_ene=0.0;
    auto num_iter = schd.cdfciIter;
    std::cout << "start optimization" << std::endl;
    for(int k=0; k<num_iter; k++) {
      for(int i=0; i<1000; i++) {
        auto dx = cdfci::CoordinateUpdate(thisDet, wfn, ene, E0, I1, I2, coreE);
        auto column = cdfci::getSubDets(thisDet, wfn, nelec, sample);
        cdfci::civectorUpdate(column, wfn, dx, ene, I1, I2, coreE, thresh, sample);
        //sub = cdfci::getSubDets(thisDet, wfn, nelec);
        thisDet = cdfci::CoordinatePickGcdGrad(column, ene.second);
      }
      auto curr_ene = ene.first/ene.second;
      std::cout << "var size : " << wfn.size() << " ";
      std::cout << k*1000 << " Current Energy: " <<  std::setw(16) <<std::setprecision(12) << curr_ene+coreEbkp << ". norm: " << std::setw(8) << std::setprecision(5) << ene.second << " prev ene " << std::setw(16) << std::setprecision(12) << prev_ene+coreEbkp << " time now " << std::setprecision(4) << getTime()-startofCalc << std::endl;
      if (std::abs(curr_ene-prev_ene) < schd.dE) {
        break;
      }
      prev_ene = curr_ene;
    }
  coreE=coreEbkp;
  return;
}

// no longer usable
vector<double> cdfci::DoVariational(vector<MatrixXx>& ci, vector<Determinant> & Dets, schedule& schd, twoInt& I2, twoIntHeatBathSHM& I2HB, vector<int>& irrep, oneInt& I1, double& coreE, int nelec, bool DoRDM) {

  int proc = 0, nprocs = 1;

  if (schd.outputlevel > 0 && commrank == 0) {
    Time::print_time("start variation");
  }

  int nroots = ci.size();
  size_t norbs = I2.Direct.rows();
  int Norbs = norbs;
  
  // assume we keep one vector version of determinants and ci vectors, and one hash map version of it.

  pout << format("%4s %4s  %10s  %10.2e   %18s   %9s  %10s\n") % ("Iter") %
            ("Root") % ("Eps1 ") % ("#Var. Det.") % ("Energy") %
            ("#Davidson") % ("Time(s)");
  
  int prevSize = 0;
  int iterstart = 0;

  vector<Determinant> SortedDetsvec;
  SortedDetsvec = Dets;
  std::sort(SortedDetsvec.begin(), SortedDetsvec.end());
  int SortedDetsSize = SortedDetsvec.size();
  int DetsSize = Dets.size();

  hash_det wavefunction;
  for (int i=0; i<SortedDetsSize; i++) {
    double civec = ci[i](0);
    double norm = civec*civec;
    value_type value (SortedDetsvec[i], {0.0, 0.0});
    std::array<double, 2>  val {0.0,0.0};
    wavefunction.insert({SortedDetsvec[i], val});
  }
  //wavefunction[SortedDetsvec[0]]={1.0, SortedDetsvec[0].Energy(I1,I2,coreE)};
  //std::cout << wavefunction.begin()->first << std::endl;
  Determinant HF = Dets[0];
  CItype e0 = SortedDetsvec[0].Energy(I1,I2,coreE);
  vector<double> E0(nroots, abs(e0));
  //pair<double, double> ene = {e0, 1.0};
  //wavefunction[HF] = {1.0, e0};
  pair<double, double> ene={0.0,0.0};
  wavefunction[HF]={0.0,0.0};
  for (int iter = iterstart; iter < schd.epsilon1.size(); iter++) {
    double epsilon1 = schd.epsilon1[iter];
    
    CItype zero = 0.0;
    if (schd.outputlevel > 0) {
      pout << format("#-------------Iter=%4i---------------") % iter << endl;
    }

    // grow variational space
    auto SortedDetsSize = SortedDetsvec.size();
    set<Determinant> new_dets;
    std::cout << "time before grow variational space " << std::setw(10) << getTime()-startofCalc << std::endl;
    for (int i = 0; i < SortedDetsSize; i++) {
      double norm = ene.second > 1e-10 ? ene.second : 1.0;
      double civec = wavefunction[SortedDetsvec[i]][0];
      civec = civec / sqrt(norm);
      if(iter==0) {
        civec = 1.0;
        //std::cout << civec << " " << norm << std::endl;
      }
      array<double, 2> zero_out = {0.0,0.0};
      wavefunction[SortedDetsvec[i]] = zero_out;
      //cdfci::getDeterminantsVariational(SortedDetsvec[i], epsilon1/abs(civec), civec, zero, I1, I2, I2HB, irrep, coreE, E0[0], wavefunction, new_dets, schd, 0, nelec);
    }
    std::cout << "time after grow variational space " 
    <<std:: setw(10) << getTime()-startofCalc << std::endl;
    auto&wfn = wavefunction;
    std::cout << "wfn size:  " << wfn.size() << std::endl;
    std::cout << "new dets: " << new_dets.size() << std::endl;
    ene = {0.0,0.0};
//    cdfci::cdfciSolver(wavefunction, HF, schd, ene, I1, I2, coreE, E0, nelec, epsilon1, true);
    exit(0);
    SortedDetsvec.clear(); 
    for (auto iter:wavefunction) {
      SortedDetsvec.push_back(iter.first);
    }
  }
  return E0;
}

void civectorUpdateNoSample(pair<double, double>& ene, vector<int>& column, double dx, vector<Determinant>& dets, vector<double>& x_vector, vector<double>& z_vector, cdfci::DetToIndex& det_to_index, oneInt& I1, twoInt& I2, double& coreE) {
  auto deti = dets[column[0]];
  size_t orbDiff;
  double hij;
  hij = deti.Energy(I1, I2, coreE);
  auto dz = hij * dx;
  //z_vector[column[0]] += dz;
  //auto x = x_vector[column[0]];
  //ene.first += (dx * hij * dx + 2 * dx * hij * x);
  //ene.second += dx * dx + 2 * dx * x;
  //x_vector[column[0]] += dx;
  int num_iter = column.size();
  double norm = 0.0;
  //#pragma omp parallel for private(x), reduction (+:norm)
  for (int i = 1; i < num_iter; i++) {
    auto detj = dets[column[i]];
    auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
    auto dz = dx * hij;
    auto x = x_vector[column[i]];
    z_vector[column[i]] += dz;
    norm += 2.*x*dz;
  }
  #pragma omp atomic
  ene.first += norm;
  return;
}

double CoordinateUpdate(Determinant& det, double x, double z, double xx, oneInt& I1, twoInt& I2, double& coreE) {
  double result;
  double dx = 0.0;
  size_t orbDiff;
  
  auto dA = det.Energy(I1, I2, coreE);
  dA=-dA;
  // Line Search, cced from original cdfci code.
  auto p1 = xx - x * x - dA;
  auto q = z + dA * x;  //
  auto p3 = p1/3;
  auto q2 = q/2;
  auto d = p3 * p3 * p3 + q2 * q2;
  double rt = 0;
  //std::cout << "p1: " << p1 << " q: " << q << std::endl;
  const double pi = atan(1.0) * 4;

  if (d >= 0) {
    auto qrtd = sqrt(d);
    rt = cbrt(-q2 + qrtd) + cbrt(-q2 - qrtd);
  }
  else {
    auto qrtd = sqrt(-d);
    if (q2 >= 0) {
      rt = 2 * sqrt(-p3) * cos((atan2(-qrtd, -q2) - 2*pi)/3.0);      
    }
    else {
      rt = 2 * sqrt(-p3) * cos(atan2(qrtd, -q2)/3.0);
    }
  }
  dx = rt - x;
// Newton iteration to improve accuracy
  auto dxn = dx - (dx*(dx*(dx + 3*x) + (3*x*x + p1)) + p1*x + q + x*x*x)
             / (dx*(3*dx + 6*x) + 3*x*x + p1);

  const double depsilon = 1e-12;
  const int max_iter = 10;
  int iter = 0;
  while (fabs((dxn - dx)/dx) > depsilon && iter < max_iter)
  {
      dx = dxn;
      dxn = dx - (dx*(dx*(dx + 3*x) + (3*x*x + p1)) + p1*x + q + x*x*x)
          / (dx*(3*dx + 6*x) + 3*x*x + p1);
      ++iter;
  }
  return dx;
}

// solve for x^3 + p1 x + q = 0.
double cubicSolve(double x, double p1, double q) {
  double rt = 0;
  double dx = 0.0;
  double p3 = p1 / 3.0;
  double q2 = q / 2.0;
  auto d = p3 * p3 * p3 + q2 * q2;
  //std::cout << "p1: " << p1 << " q: " << q << std::endl;
  const double pi = atan(1.0) * 4;

  if (d >= 0) {
    auto qrtd = sqrt(d);
    rt = cbrt(-q2 + qrtd) + cbrt(-q2 - qrtd);
  }
  else {
    auto qrtd = sqrt(-d);
    if (q2 >= 0) {
      rt = 2 * sqrt(-p3) * cos((atan2(-qrtd, -q2) - 2*pi)/3.0);      
    }
    else {
      rt = 2 * sqrt(-p3) * cos(atan2(qrtd, -q2)/3.0);
    }
  }
  dx = rt - x;
// Newton iteration to improve accuracy
  auto dxn = dx - (dx*(dx*(dx + 3*x) + (3*x*x + p1)) + p1*x + q + x*x*x)
             / (dx*(3*dx + 6*x) + 3*x*x + p1);

  const double depsilon = 1e-12;
  const int max_iter = 10;
  int iter = 0;
  while (fabs((dxn - dx)/dx) > depsilon && iter < max_iter)
  {
      dx = dxn;
      dxn = dx - (dx*(dx*(dx + 3*x) + (3*x*x + p1)) + p1*x + q + x*x*x)
          / (dx*(3*dx + 6*x) + 3*x*x + p1);
      ++iter;
  }
  return dx;
}

vector<double> CoordinateUpdateMultiRoot(Determinant& det, vector<double> x, vector<double>& z, vector<vector<double>>& xx, oneInt& I1, twoInt& I2, double& coreE) {
  auto dA = det.Energy(I1, I2, coreE);
  const int nroots = x.size();
  vector<double> dx(nroots, 0);
  for (int i=0; i<nroots; i++) {
    double p1 = xx[i][i] - 2.*x[i]*x[i] + dA;
    double q = z[i] - dA * x[i]-xx[i][i]*x[i]+x[i]*x[i]*x[i];
    for (int j=0; j<nroots; j++) {
      p1 += x[j] * x[j];
      q += (xx[i][j]*x[j] - x[i]*x[j]*x[j]);
    }
    dx[i] = cubicSolve(x[i], p1, q);
    cout << x[i] << " " << p1 << " " << q << " " << dx[i] << endl;
    xx[i][i] += dx[i]*dx[i]+2.*dx[i]*x[i];
    for (int j=0; j<nroots; j++) {
      if (j!=i) {
        xx[i][j] += dx[i]*x[j];
        xx[j][i] += dx[i]*x[j];
      }
    }
    x[i] += dx[i];
  }
  return dx;
}

double CoordinateUpdateIthRoot(Determinant& det, int& iroot, vector<double>& x, vector<double>& z, vector<vector<float128>>& xx, oneInt& I1, twoInt& I2, double& coreE) {
  auto dA = det.Energy(I1, I2, coreE);
  const int nroots = x.size();
  int i = iroot;
  double p1 = xx[i][i] - x[i]*x[i] + dA;
  double q = z[i] - dA * x[i];
  for (int j=0; j<i; j++) {
    p1 += x[j] * x[j];
    q += (xx[i][j]*x[j] - x[i]*x[j]*x[j]);
  }
  double dx = cubicSolve(x[i], p1, q);
 
  return dx;
}

// need to resize the result vector
void getSubDetsNoSample(vector<Determinant>& dets, vector<int>& column, cdfci::DetToIndex& det_to_index, int this_index, int nelec, twoIntHeatBathSHM& I2hb, double epsilon) {
  auto det = dets[this_index];

  int norbs = det.norbs;
  int nclosed = nelec;
  int nopen = norbs-nclosed;
  vector<int> closed(nelec, 0);
  vector<int> open(nopen, 0);
  det.getOpenClosed(open, closed);
  vector<int> result(1+nopen*nclosed+nopen*nopen*nclosed*nclosed, -1);
  result[0] = this_index;
  //#pragma omp parallel for schedule(dynamic) shared(result, det_to_index)
  for (int ia=0; ia<nopen*nclosed; ia++) {
    int i=ia/nopen, a=ia%nopen;
    if (closed[i]%2 == open[a]%2) {
      Determinant di = det;
      di.setocc(open[a], true);
      di.setocc(closed[i], false);
      auto iter = det_to_index.find(di);
      if (iter != det_to_index.end()) {
        result[ia+1] = iter->second;
      }
    }
  }

  //get all existing double excitations
  //#pragma omp parallel for schedule(dynamic) shared(result, det_to_index)
  for(int ij=0; ij<nclosed*nclosed; ij++) {
    int i=ij/nclosed, j=ij%nclosed;
    if(i<=j) continue;
    int I = closed[i], J = closed[j];
    int X = max(I/2, J/2), Y = min(I/2, J/2);
    
    int pairIndex = X*(X+1)/2+Y;
    size_t start = I%2==J%2 ? I2hb.startingIndicesSameSpin[pairIndex]   : I2hb.startingIndicesOppositeSpin[pairIndex];
    size_t end   = I%2==J%2 ? I2hb.startingIndicesSameSpin[pairIndex+1] : I2hb.startingIndicesOppositeSpin[pairIndex+1];
    float* integrals  = I%2==J%2 ?  I2hb.sameSpinIntegrals : I2hb.oppositeSpinIntegrals;
    short* orbIndices = I%2==J%2 ?  I2hb.sameSpinPairs     : I2hb.oppositeSpinPairs;
    /*for (size_t index = start; index < end; index++) {
      //if (fabs(integrals[index]) < epsilon) break;
      int a = 2* orbIndices[2*index] + closed[i]%2, b= 2*orbIndices[2*index+1]+closed[j]%2;
      if (!(det.getocc(a) || det.getocc(b))) {
        Determinant di = det;
        di.setocc(a, true);
        di.setocc(b, true);
        di.setocc(I, false);
        di.setocc(J, false);
        auto iter = det_to_index.find(di);
        if (iter != det_to_index.end()) {
          result[1+nopen*nclosed+ij*nopen*nopen+a*nopen+b] = iter->second;
        }
      }
    }*/
    
    for (int kl=0;kl<nopen*nopen;kl++) {
      int k=kl/nopen;
      int l=kl%nopen;
      if(k<=l) continue;
      int K=open[k], L=open[l];
      int a = max(K,L), b = min(K,L);
      if (a%2+b%2-I%2-J%2 != 0) {
        continue;
      }
      Determinant di = det;
      di.setocc(a, true);
      di.setocc(b, true);
      di.setocc(I, false);
      di.setocc(J, false);
      auto iter = det_to_index.find(di);
      if (iter != det_to_index.end()) {
        result[1+nopen*nclosed+ij*nopen*nopen+kl] = iter->second;
      }
    }
  }
  column.clear();
  column.push_back(result[0]);
  //#pragma omp parallel for
  for (int i = 1; i < 1+nopen*nclosed+nopen*nopen*nclosed*nclosed; i++) {
    if (result[i]>=0) 
    {
      column.push_back(result[i]);
    }
  }
  return;
}

int CoordinatePickGcdGradOmp(vector<int>& column, vector<double>& x_vector, vector<double>& z_vector, vector<pair<double, double>>& ene, int num_threads=1, int thread_id = 0) {
  double max_abs_grad = 0.0;
  double norm = ene[0].second;
  int column_size = column.size();
  int result;
  for(int i = 0; i < column_size; i++) {
    if (column[i] % num_threads == thread_id) {
      auto x = x_vector[column[i]];
      auto z = z_vector[column[i]];
      auto abs_grad = std::abs(x*norm+z);
      if (abs_grad > max_abs_grad) {
        max_abs_grad = abs_grad;
        result = column[i];
      }
    }
  }
  //cout << setw(4) << num_threads << setw(4) << thread_id << setw(4) << result << endl;
  // only optimize ground state
  /*int num_threads;
  vector<double> max_abs_grad;
  vector<int> local_results;
  double norm = ene[0].second;
  #pragma omp parallel 
  {
    num_threads = omp_get_num_threads();
  }
    max_abs_grad.resize(num_threads);
    local_results.resize(num_threads);
  #pragma omp parallel for
  for(int i = 1; i < column.size(); i++) {
    int thread_id = omp_get_thread_num();
    int pairIndex = X*(X+1)/2+Y;
    auto x = x_vector[column[i]];
    auto z = z_vector[column[i]];
    auto abs_grad = std::abs(x*norm+z);
    if (abs_grad > max_abs_grad[thread_id]) {
      max_abs_grad[thread_id] = abs_grad;
      local_results[thread_id] = column[i];
    }
  }lo
  auto result = local_results[0];
  auto result_idx = 0;
  for (int i = 1; i<num_threads; i++) {
    if (max_abs_grad[result_idx] < max_abs_grad[i]) {
      result = local_results[i];
      result_idx = i;
    }
  }*/
  return result;
}

int CoordinatePickGcdGradOmpMultiRoot(vector<int>& column, vector<double>& x_vector, vector<double>& z_vector, vector<pair<double, double>>& ene) {
  int num_threads;
  vector<double> max_abs_grad;
  vector<int> local_results;
  // only optimize ground state
  double norm = ene[0].second;
  #pragma omp parallel 
  {
    num_threads = omp_get_num_threads();
  }
    max_abs_grad.resize(num_threads);
    local_results.resize(num_threads);
  #pragma omp parallel for
  for(int i = 1; i < column.size(); i++) {
    int thread_id = omp_get_thread_num();
    auto x = x_vector[column[i]];
    auto z = z_vector[column[i]];
    auto abs_grad = std::abs(x*norm+z);
    if (abs_grad > max_abs_grad[thread_id]) {
      max_abs_grad[thread_id] = abs_grad;
      local_results[thread_id] = column[i];
    }
  }
  auto result = local_results[0];
  auto result_idx = 0;
  for (int i = 1; i<num_threads; i++) {
    if (max_abs_grad[result_idx] < max_abs_grad[i]) {
      result = local_results[i];
      result_idx = i;
    }
  }
  return result;
}

vector<pair<float128, float128>> precondition(vector<double>& x_vector, vector<double>& z_vector, vector<vector<float128>>& xx, vector<MatrixXx>& ci, cdfci::DetToIndex& det_to_index, vector<Determinant>& dets, vector<double>& E0, oneInt& I1, twoInt& I2, twoIntHeatBathSHM& I2HB, double coreE, double epsilon) {
  int nelec = dets[0].Noccupied();
  const int norbs = dets[0].norbs;
  const int nclosed = dets[0].Noccupied();
  const int nopen = norbs-nclosed;
  vector<pair<float128, float128>> result;
  vector<int> column;
  int nroots = ci.size();
  
  auto start = getTime();
  for (int iroot = 0; iroot < nroots; iroot++) {
    int x_size = ci[iroot].rows();
    int z_size = det_to_index.size();
    double norm = sqrt(abs(E0[iroot]-coreE));
    auto result_iroot = pair<float128, float128>(0.0, 0.0);
    for (int i = 0; i < x_size; i++) {
      auto dx = ci[iroot](i, 0) * norm;
      result_iroot.second += dx*dx;
      x_vector[i*nroots+iroot] = dx;
    }
    double xz = 0.0;
    #pragma omp parallel for reduction(+:xz)
    for (int i = 0; i < x_size; i++) {
      if(i%10000==0) cout << i << " " << getTime()-start << "\n";
      auto deti = dets[i];
      vector<int> closed(nelec, 0);
      vector<int> open(nopen, 0);
      deti.getOpenClosed(open, closed);
      auto hij = deti.Energy(I1, I2, coreE);
      auto xi = x_vector[i*nroots+iroot];
      xz += xi * xi * hij;
      #pragma omp atomic
      z_vector[i*nroots+iroot] += hij*xi;
      for (int ia = 0; ia < nopen*nclosed; ia++) {
        int i = ia / nopen, a = ia % nopen;
        if (closed[i]%2 == open[a]%2) {
          auto detj = deti;
          detj.setocc(open[a], true);
          detj.setocc(closed[i], false);
          auto iter = det_to_index.find(detj);
          if (iter != det_to_index.end()) {
            auto j = iter->second;
            auto detj = dets[j];
            auto xj = x_vector[j*nroots+iroot];
            size_t orbDiff;
            hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
            xz += xi * xj * hij;
            #pragma omp atomic
            z_vector[j*nroots+iroot] += xi*hij;
          }
        }
      }
      for(int ij=0; ij<nclosed*nclosed; ij++) {
        int i=ij/nclosed, j=ij%nclosed;
        if(i<=j) continue;
        int I = closed[i], J = closed[j];
        int X = max(I/2, J/2), Y = min(I/2, J/2);
    
        int pairIndex = X*(X+1)/2+Y;
        size_t start = I%2==J%2 ? I2HB.startingIndicesSameSpin[pairIndex]   : I2HB.startingIndicesOppositeSpin[pairIndex];
        size_t end   = I%2==J%2 ? I2HB.startingIndicesSameSpin[pairIndex+1] : I2HB.startingIndicesOppositeSpin[pairIndex+1];
        float* integrals  = I%2==J%2 ?  I2HB.sameSpinIntegrals : I2HB.oppositeSpinIntegrals;
        short* orbIndices = I%2==J%2 ?  I2HB.sameSpinPairs     : I2HB.oppositeSpinPairs;
        for (size_t index = start; index < end; index++) {
          //if (fabs(integrals[index]) < screen) break;
          int a = 2* orbIndices[2*index] + closed[i]%2, b= 2*orbIndices[2*index+1]+closed[j]%2;
          if(deti.getocc(a) || deti.getocc(b)) continue; 
          Determinant detj = deti;
          detj.setocc(a, true);
          detj.setocc(b, true);
          detj.setocc(I, false);
          detj.setocc(J, false);
          auto iter = det_to_index.find(detj);
          if (iter != det_to_index.end()) {
          auto j = iter->second;
          auto detj = dets[j];
          auto xj = x_vector[j*nroots+iroot];
          size_t orbDiff;
          hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
          xz += xi * xj * hij;
          #pragma omp atomic
          z_vector[j*nroots+iroot] += xi*hij;
          }
        }
      }
    }
    #pragma omp barrier
    xx[iroot][iroot] = result_iroot.second;
    result_iroot.first = xz;
    result.push_back(result_iroot);
  }
  auto residual = cdfci::compute_residual(x_vector, z_vector, result);
  for (int iroot=0; iroot<nroots; iroot++) {
    cout << double(result[iroot].first/result[iroot].second) + coreE<< residual[iroot] << endl;
  }
  for (int iroot=0; iroot<nroots; iroot++) {
    cout << cdfci::compute_residual(x_vector, z_vector, result, iroot) << endl;
  }
  return result;
}
vector<double> cdfci::compute_residual(vector<double>& x, vector<double>& z, vector<pair<float128, float128>>& ene) {
  const int nroots = ene.size();
  const int size = x.size()/nroots;
  vector<double> residual(nroots, 0.0);
  vector<double> energy(nroots, 0.0);
  for (int iroot = 0; iroot < nroots; iroot++) {
    double res_iroot = 0.0;
    energy[iroot] = ene[iroot].first / ene[iroot].second;
    #pragma omp parallel for reduction(+:res_iroot)
    for (int i=0; i<size; i++) {
      auto tmp = z[i*nroots+iroot]-energy[iroot] * x[i*nroots+iroot];
      res_iroot += tmp*tmp;
    }
    #pragma omp barrier
    residual[iroot] = res_iroot;
  }
  for(int iroot = 0; iroot<nroots; iroot++) {
    residual[iroot] /= ene[iroot].second;
  }
  return residual;
}

double cdfci::compute_residual(vector<double>& x, vector<double>& z, vector<pair<float128, float128>>& ene, int& iroot) {
  const int nroots = ene.size();
  const int size = x.size();
  double residual = 0.0;
  double energy = ene[iroot].first / ene[iroot].second;
  #pragma omp parallel  reduction(+:residual)
  for (int i=iroot; i<size; i+=nroots) {
    auto tmp = z[i]-energy * x[i];
      residual += tmp*tmp;
  }
  #pragma omp barrier
  residual /= ene[iroot].second;
  return residual;
}
void cdfci::solve(schedule& schd, oneInt& I1, twoInt& I2, twoIntHeatBathSHM& I2HB, vector<int>& irrep, double& coreE, vector<double>& E0, vector<MatrixXx>& ci, vector<Determinant>& dets) {
  DetToIndex det_to_index;
  int iter;
  bool converged;

  int thread_id;
  int thread_num;
  #pragma omp parallel
  {
    thread_num = omp_get_num_threads();
  }
  if (schd.restart) {
    char file[5000];
    sprintf(file, "%s/%d-variational.bkp", schd.prefix[0].c_str(), commrank);
    std::ifstream ifs(file, std::ios::binary);
    boost::archive::binary_iarchive load(ifs);
    load >> iter >> dets;
    ci.resize(1, MatrixXx(dets.size(), 1));

    load >> ci;
    load >> E0;
    load >> converged;
    pout << "Load converged: " << converged << endl;
  }
  int start_index = ci[0].rows();
  int dets_size = dets.size();
  double coreEbkp = coreE;
  coreE = 0.0;
  
  for (int i = 0; i < dets_size; i++) {
    det_to_index[dets[i]] = i;
  }

  const double epsilon1 = schd.epsilon1[schd.cdfci_on];
  const int nelec = dets[0].Noccupied();
  const double zero = 0.0;

  for (int i = 0; i < dets_size; i++) {
    auto civec = ci[0](i, 0);
    cdfci::getDeterminantsVariational(dets[i], epsilon1/abs(civec), civec, zero, I1, I2, I2HB, irrep, coreE, E0[0], det_to_index, schd, 0, nelec);
    if (i%10000 == 0) {
      cout << "curr iter " << i << " dets size " << det_to_index.size() << endl;
    }
  }
  dets.resize(det_to_index.size());
  for (auto it : det_to_index) {
    dets[it.second] = it.first;
  }
  dets_size = dets.size();
  cout << " " << dets_size << endl;
  cout << "build det to index" << endl;
  for (int i = 0; i < dets.size(); i++) {
    det_to_index[dets[i]] = i;
    if (i % 10000000 == 0) {
      cout << i << " dets constructed" << endl;
    }
  }
  
  cout << "starts to precondition" << endl;
  // ene stores the rayleigh quotient quantities.
  int nroots = schd.nroots;
  vector<pair<float128, float128>> ene(nroots, make_pair(0.0, 0.0));
  vector<vector<float128>> xx(nroots, vector<float128>(nroots, 0.0));
  vector<double> x_vector(dets_size * nroots, zero), z_vector(dets_size * nroots, zero);
  auto start_time = getTime();
  ene = precondition(x_vector, z_vector, xx, ci, det_to_index, dets, E0, I1, I2, I2HB, coreE, epsilon1);
  start_index = 0;
  auto num_iter = schd.cdfciIter;
  vector<int> this_det_idx(thread_num, 0);
  vector<vector<double>> dxs(thread_num);
  int det_idx = 0;
  vector<int> column;

  #pragma omp parallel
  {
    thread_id = omp_get_thread_num();
    this_det_idx[thread_id] = start_index + thread_id;
  }
  cout << "start to optimize" << endl;

    auto prev_ene = ene;
    start_time = getTime();
    for (int iter = 1; iter*thread_num <= num_iter; iter++) {
      
      // initialize dx on each thread
      {
        /*vector<double> x;
        vector<double> z;
        double& xx = ene[iroot].second;
        double& xz = ene[iroot].first;
        size_t orbDiff;
        for (int thread = 0; thread < thread_num; thread++) {
          x.push_back(x_vector[this_det_idx[thread]]);
          z.push_back(z_vector[this_det_idx[thread]]);
        }
        for (int thread = 0; thread < thread_num; thread++) {
          auto i = this_det_idx[thread];
          dxs[thread] = CoordinateUpdate(dets[i], x[thread], z[thread], ene[iroot].second, I1, I2, coreE);
          auto dx = dxs[thread];
          auto hij = dets[i].Energy(I1, I2, coreE);
          auto x = x_vector[i];
          ene[iroot].first += (dx * hij * dx + 2 * dx * hij * x);
          ene[iroot].second += dx * dx + 2 * dx * x;
          x_vector[i] += dx;
          z_vector[i] += hij * dx;
          for (int thread_j = thread+1; thread_j < thread_num; thread_j++) {
            auto j = this_det_idx[thread_j];
            if (dets[i].ExcitationDistance(dets[j]) > 2 || i==j) continue;
            else {
              auto hij = Hij(dets[i], dets[j], I1, I2, coreE, orbDiff);
              z[thread_j] += dx * hij;
            }
          }
        }*/

        vector<vector<double>> x;
        vector<vector<double>> z;
        size_t orbDiff;
        for (int thread = 0; thread < thread_num; thread++) {
          auto x_first = x_vector.begin() + nroots*this_det_idx[thread];
          auto x_last = x_first + nroots;
          auto z_first = z_vector.begin() + nroots*this_det_idx[thread];
          auto z_last = z_first + nroots;
          x.push_back(vector<double>(x_first, x_last));
          z.push_back(vector<double>(z_first, z_last));
        }
        for (int thread = 0; thread < thread_num; thread++) {
          auto i = this_det_idx[thread];
          dxs[thread] = vector<double>(nroots, 0.0);//CoordinateUpdateMultiRoot(dets[i], x[thread], z[thread], xx, I1, I2, coreE);
          auto hij = dets[i].Energy(I1, I2, coreE);
          auto dx = dxs[thread];
          
          for (int iroot=0; iroot<nroots; iroot++) {
            ene[iroot].first += dx[iroot]*hij*dx[iroot]+2.0*dx[iroot]*hij*x_vector[i*nroots+iroot];
            ene[iroot].second += dx[iroot]*dx[iroot]+2.0*dx[iroot]*x_vector[i*nroots+iroot];
            x_vector[i*nroots+iroot] += dx[iroot];
            //cout << dx[iroot] << " " << x_vector[i*nroots+iroot] << " " << x[iroot] << endl;
            z_vector[i*nroots+iroot] += hij*dx[iroot];
          }
          for(int thread_j = thread+1; thread_j < thread_num; thread_j++) {
            auto j = this_det_idx[thread_j];
            if (dets[i].ExcitationDistance(dets[j]) > 2 || i==j) continue;
            else {
              auto hij = Hij(dets[i], dets[j], I1, I2, coreE, orbDiff);
              for (int iroot=0; iroot<nroots; iroot++) {
                z[thread_j][iroot] += dx[iroot]*hij;
              }
            }
          }
        }
        // this step is because, although we are updating all the walkers simultaneously
        // what in fact want to mimic the result of sequential update.
        // but, we are updating all the x_vectors at the very beginning.
        // the "later" x_vectors are updated earlier than expected, and will have an
        // influence on the xz/ene.first term. So this influence needs to be deducted.
        // xz += 2*x*dz. So 2*dx*dz should be deducted. Only terms with non vanishing dx has a contribution.
        // which means that, only the dets with a to be updated x[j] will be affected.,
        // xz -= 2*dx[j]*dz[i] dz[i] = hij*dx[i], with j > i. ,
        // Because only when j > i, the future happening update is affecting the past.
        for (int thread_i=0; thread_i < thread_num; thread_i++) {
          auto i = this_det_idx[thread_i];
          auto deti = dets[i];
          auto dxi = dxs[thread_i];
          for(int thread_j = thread_i+1; thread_j < thread_num; thread_j++) {
            auto j = this_det_idx[thread_j];
            auto detj = dets[j];
            if (deti.ExcitationDistance(detj) > 2 || i==j) continue;
            else {
              auto dxj = dxs[thread_j];
              auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
              for(int iroot=0; iroot<nroots; iroot++) {
                ene[iroot].first -= 2.*dxj[iroot]*dxi[iroot]*hij;
              }
            }
          }
        }
      }

      const int norbs = dets[0].norbs;
      const int nclosed = dets[0].Noccupied();
      const int nopen = norbs-nclosed;
      
      #pragma omp parallel private(column)
      {
        int thread_id = omp_get_thread_num();
        int det_idx = this_det_idx[thread_id];
        auto deti = dets[det_idx];
        vector<int> closed(nelec, 0);
        vector<int> open(nopen, 0);
        deti.getOpenClosed(open, closed);
        //const auto xx = ene[iroot].second;
        double max_abs_grad = 0.0;
        int selected_det = det_idx;
        //getSubDetsNoSample(dets, column, det_to_index, det_idx, nelec);
        //auto dx = CoordinateUpdate(dets[det_idx], x_vector[det_idx], z_vector[det_idx], ene[iroot].second, I1, I2, coreE);
        auto dx = dxs[thread_id];
        auto det_energy = deti.Energy(I1, I2, coreE);
        auto z_begin = z_vector.begin()+nroots*det_idx;
        auto z_end = z_begin+nroots;
        vector<double> dz(nroots, 0.0);

        auto x = x_vector[det_idx];
        vector<double> norm(nroots, 0.0);
        //for (int entry = 1; entry < column.size(); entry++) {
        for (int ia = 0; ia < nopen*nclosed; ia++) {
          int i = ia / nopen, a = ia % nopen;
          if (closed[i]%2 == open[a]%2) {
            auto detj = deti;
            detj.setocc(open[a], true);
            detj.setocc(closed[i], false);
            auto iter = det_to_index.find(detj);
            if (iter != det_to_index.end()) {
              auto j_idx = iter->second;
              size_t orbDiff;
              auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
              //transform(dx.begin(), dx.end(), dz.begin(), [&hij](auto &dxi){return hij*dxi;});
              for(int iroot=0; iroot<nroots;iroot++) {
                dz[iroot] = hij*dx[iroot];
              }
              
              for(int iroot=0; iroot < nroots; iroot++) {
                #pragma omp atomic
                z_vector[j_idx*nroots+iroot] += dz[iroot];
                norm[iroot] += 2.0*x_vector[j_idx*nroots+iroot]*dz[iroot];
              }
              if (j_idx % thread_num == thread_id) {
                double abs_grad = 0.0;
                for (int iroot=0; iroot<nroots; iroot++) {
                  abs_grad += z_vector[nroots*j_idx+iroot];
                  for (int jroot=0; jroot<nroots; jroot++) 
                  abs_grad += xx[iroot][jroot] * x_vector[nroots*j_idx+jroot];
                }
                abs_grad = abs(abs_grad);
                if (abs_grad > max_abs_grad) {
                  max_abs_grad = abs_grad;
                  selected_det = j_idx;
                } 
              }
            }
          }
        }
        for(int ij=0; ij<nclosed*nclosed; ij++) {
          int i=ij/nclosed, j=ij%nclosed;
          if(i<=j) continue;
          int I = closed[i], J = closed[j];
          for (int kl=0;kl<nopen*nopen;kl++) {
            int k=kl/nopen;
            int l=kl%nopen;
            if(k<=l) continue;
            int K=open[k], L=open[l];
            int a = max(K,L), b = min(K,L);
            if (a%2+b%2-I%2-J%2 != 0) {
              continue;
            }
            auto detj = deti;
            detj.setocc(a, true);
            detj.setocc(b, true);
            detj.setocc(I, false);
            detj.setocc(J, false);
            auto iter = det_to_index.find(detj);
            if (iter != det_to_index.end()) {
              auto j_idx = iter->second;
              size_t orbDiff;
              auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
              //transform(dx.begin(), dx.end(), dz.begin(), [&hij](auto &dxi){return hij*dxi;});
              for(int iroot=0; iroot<nroots;iroot++) {
                dz[iroot] = hij*dx[iroot];
              }
              auto x_begin = x_vector.begin()+nroots*j_idx;
              auto x_end = x_begin+nroots;
              z_begin = z_vector.begin() + nroots*j_idx;
              z_end = z_begin+nroots;
              for(int iroot=0; iroot < nroots; iroot++) {
                #pragma omp atomic
                z_vector[j_idx*nroots+iroot] += dz[iroot];
                norm[iroot] += 2.0*x_vector[j_idx*nroots+iroot]*dz[iroot];
              }
              if (j_idx % thread_num == thread_id) {
                double abs_grad = 0.0;
                for (int iroot=0; iroot<nroots; iroot++) {
                  abs_grad += z_vector[nroots*j_idx+iroot];
                  for (int jroot=0; jroot<nroots; jroot++) 
                  abs_grad += xx[iroot][jroot] * x_vector[nroots*j_idx+jroot];
                }
                abs_grad = abs(abs_grad);
                if (abs_grad > max_abs_grad) {
                  max_abs_grad = abs_grad;
                  selected_det = j_idx;
                }
              }
            }
          }
        }
        this_det_idx[thread_id] = selected_det;//CoordinatePickGcdGradOmp(column, x_vector, z_vector, ene, thread_num, thread_id);
        for (int iroot=0; iroot<nroots; iroot++) {
          #pragma omp atomic
          ene[iroot].first += norm[iroot];
        }
      }
      #pragma omp barrier
      // now some logical codes, print out information and decide when to exit etc.
      if (iter*thread_num%schd.report_interval < thread_num || iter == num_iter) {
        auto residual = cdfci::compute_residual(x_vector, z_vector, ene);
        for(int iroot = 0; iroot < nroots; iroot++) {
          double curr_energy = ene[iroot].first/ene[iroot].second;
          double prev_energy = prev_ene[iroot].first/prev_ene[iroot].second;
          cout << setw(10) << iter * thread_num << setw(20) <<std::setprecision(14) << curr_energy+coreEbkp << setw(20) <<std::setprecision  (14) << prev_energy+coreEbkp;
          cout << std::setw(12) << std::setprecision(4) << scientific << getTime()-start_time << defaultfloat;
          cout << std::setw(12) << std::setprecision(4) << scientific << residual[iroot] << defaultfloat << endl;
          cout << endl;
        }
        double residual_sum = 0.0;
        for(auto & res_iroot : residual) residual_sum += res_iroot;
        if (residual_sum < schd.cdfciTol*nroots) {
          break;
        }
        prev_ene = ene;
      }
    }
  /*
  vector<vector<double>> xzx(nroots, vector<double>(nroots, 0.0));
  for(int iroot=0; iroot<nroots; iroot++) {
    xzx[iroot][iroot] = ene[iroot].first;
    for(int jroot=iroot+1; jroot<nroots;jroot++) {
      for(int i=0; i<dets_size; i++) {
        xzx[iroot][jroot] += x_vector[i*nroots+iroot]*z_vector[i*nroots+jroot];
        xzx[jroot][iroot] += x_vector[i*nroots+jroot]*z_vector[i*nroots+iroot];
      }
    }
  }
  for(int iroot=0; iroot<nroots;iroot++) {
   for(int jroot=0; jroot<nroots; jroot++) {
     cout << setprecision(10)<<xzx[iroot][jroot];
   }
   cout << endl;
  }*/
  coreE = coreEbkp;
  return;
}

void cdfci::sequential_solve(schedule& schd, oneInt& I1, twoInt& I2, twoIntHeatBathSHM& I2HB, vector<int>& irrep, double& coreE, vector<double>& E0, vector<MatrixXx>& ci, vector<Determinant>& dets) {
  DetToIndex det_to_index;
  int iter;
  bool converged;

  int thread_id;
  int thread_num;
  #pragma omp parallel
  {
    thread_num = omp_get_num_threads();
  }
  if (schd.restart) {
    char file[5000];
    sprintf(file, "%s/%d-variational.bkp", schd.prefix[0].c_str(), commrank);
    std::ifstream ifs(file, std::ios::binary);
    boost::archive::binary_iarchive load(ifs);

    load >> iter >> dets;
    ci.resize(1, MatrixXx(dets.size(), 1));

    load >> ci;
    load >> E0;
    load >> converged;
    pout << "Load converged: " << converged << endl;
  }
  int start_index = ci[0].rows();
  int dets_size = dets.size();
  double coreEbkp = coreE;
  coreE = 0.0;
  
  for (int i = 0; i < dets_size; i++) {
    det_to_index[dets[i]] = i;
  }

  const double epsilon1 = schd.epsilon1[schd.cdfci_on];
  const int nelec = dets[0].Noccupied();
  const double zero = 0.0;

  for (int i = 0; i < dets_size; i++) {
    auto civec = ci[0](i, 0);
    cdfci::getDeterminantsVariational(dets[i], epsilon1/abs(civec), civec, zero, I1, I2, I2HB, irrep, coreE, E0[0], det_to_index, schd, 0, nelec);
    if (i%10000 == 0) {
      cout << "curr iter " << i << " dets size " << det_to_index.size() << endl;
    }
  }
  dets.resize(det_to_index.size());
  for (auto it : det_to_index) {
    dets[it.second] = it.first;
  }
  dets_size = dets.size();
  cout << " " << dets_size << endl;
  cout << "build det to index" << endl;
  for (int i = 0; i < dets.size(); i++) {
    det_to_index[dets[i]] = i;
    if (i % 10000000 == 0) {
      cout << i << " dets constructed" << endl;
    }
  }
  
  cout << "starts to precondition" << endl;
  // ene stores the rayleigh quotient quantities.
  int nroots = schd.nroots;
  vector<pair<float128, float128>> ene(nroots, make_pair(0.0, 0.0));
  vector<vector<float128>> xx(nroots, vector<float128>(nroots, 0.0));
  vector<double> x_vector(dets_size * nroots, zero), z_vector(dets_size * nroots, zero);
  auto start_time = getTime();
  ene = precondition(x_vector, z_vector, xx, ci, det_to_index, dets, E0, I1, I2, I2HB, coreE, epsilon1);
  cout << "precondition costs: " << getTime()-start_time << "\n"; 
  start_index = 0;
  auto num_iter = schd.cdfciIter;
  vector<int> this_det_idx(thread_num, 0);
  vector<double> dxs(thread_num, 0.0);
  int det_idx = 0;
  vector<int> column;

  #pragma omp parallel
  {
    thread_id = omp_get_thread_num();
    this_det_idx[thread_id] = start_index + thread_id;
  }
  cout << "start to optimize" << endl;

    auto prev_ene = ene;
    start_time = getTime();
    for (int iroot=0; iroot < nroots; iroot++) {
    for (int iter = 1; iter*thread_num <= num_iter; iter++) {
      
      // initialize dx on each thread
      {
        vector<vector<double>> x;
        vector<vector<double>> z;
        size_t orbDiff;
        for (int thread = 0; thread < thread_num; thread++) {
          auto x_first = x_vector.begin() + nroots*this_det_idx[thread];
          auto x_last = x_first + nroots;
          auto z_first = z_vector.begin() + nroots*this_det_idx[thread];
          auto z_last = z_first + nroots;
          x.push_back(vector<double>(x_first, x_last));
          z.push_back(vector<double>(z_first, z_last));
        }
        for (int thread = 0; thread < thread_num; thread++) {
          auto i = this_det_idx[thread];
          auto idx = i*nroots+iroot;
          dxs[thread] = CoordinateUpdateIthRoot(dets[i], iroot, x[thread], z[thread], xx, I1, I2, coreE);
          //CoordinateUpdate(dets[i], x[thread][iroot], z[thread][iroot], ene[iroot].second, I1, I2, coreE);
          //CoordinateUpdateIthRoot(dets[i], iroot, x[thread], z[thread], xx, I1, I2, coreE);
          auto dx = dxs[thread];
          auto hij = dets[i].Energy(I1, I2, coreE);
          auto x = x_vector[idx];
          ene[iroot].first += (dx * hij * dx + 2 * dx * hij * x);
          ene[iroot].second += dx * dx + 2 * dx * x;
          x_vector[idx] += dx;
          z_vector[idx] += hij * dx;
          xx[iroot][iroot] += dx*dx+2*dx*x;
          for (int jroot=0; jroot<iroot; jroot++) {
            auto dij = x_vector[i*nroots+jroot] * dx;
            xx[jroot][iroot] += dij;
            xx[iroot][jroot] += dij;
          }
          for (int thread_j = thread+1; thread_j < thread_num; thread_j++) {
            auto j = this_det_idx[thread_j];
            if (dets[i].ExcitationDistance(dets[j]) > 2 || i==j) continue;
            else {
              auto hij = Hij(dets[i], dets[j], I1, I2, coreE, orbDiff);
              //z[thread_j][iroot] += dx * hij;
            }
          }
        }

        // this step is because, although we are updating all the walkers simultaneously
        // what in fact want to mimic the result of sequential update.
        // but, we are updating all the x_vectors at the very beginning.
        // the "later" x_vectors are updated earlier than expected, and will have an
        // influence on the xz/ene.first term. So this influence needs to be deducted.
        // xz += 2*x*dz. So 2*dx*dz should be deducted. Only terms with non vanishing dx has a contribution.
        // which means that, only the dets with a to be updated x[j] will be affected.,
        // xz -= 2*dx[j]*dz[i] dz[i] = hij*dx[i], with j > i. ,
        // Because only when j > i, the future happening update is affecting the past.
        for (int thread_i=0; thread_i < thread_num; thread_i++) {
          auto i = this_det_idx[thread_i];
          auto deti = dets[i];
          auto dxi = dxs[thread_i];
          for(int thread_j = thread_i+1; thread_j < thread_num; thread_j++) {
            auto j = this_det_idx[thread_j];
            auto detj = dets[j];
            if (deti.ExcitationDistance(detj) > 2 || i==j) continue;
            else {
              auto dxj = dxs[thread_j];
              auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
              ene[iroot].first -= 2.*dxj*dxi*hij;
            }
          }
        }
      }

      const int norbs = dets[0].norbs;
      const int nclosed = dets[0].Noccupied();
      const int nopen = norbs-nclosed;
      
      #pragma omp parallel private(column)
      {
        int thread_id = omp_get_thread_num();
        int det_idx = this_det_idx[thread_id];
        auto deti = dets[det_idx];
        vector<int> closed(nelec, 0);
        vector<int> open(nopen, 0);
        deti.getOpenClosed(open, closed);
        const double xx = ene[iroot].second;
        double max_abs_grad = 0.0;
        int selected_det = det_idx;
        int i_idx = det_idx * nroots+iroot;
        //getSubDetsNoSample(dets, column, det_to_index, det_idx, nelec);
        //auto dx = CoordinateUpdate(dets[det_idx], x_vector[det_idx], z_vector[det_idx], ene[iroot].second, I1, I2, coreE);
        auto dx = dxs[thread_id];
        auto det_energy = deti.Energy(I1, I2, coreE);
        auto dz = dx * det_energy;
        auto x = x_vector[i_idx];
        auto screen = 1e-10;
        double norm = 0.0;
        //for (int entry = 1; entry < column.size(); entry++) {
        for (int ia = 0; ia < nopen*nclosed; ia++) {
          int i = ia / nopen, a = ia % nopen;
          if (closed[i]%2 == open[a]%2) {
            auto detj = deti;
            detj.setocc(open[a], true);
            detj.setocc(closed[i], false);
            auto iter = det_to_index.find(detj);
            if (iter != det_to_index.end()) {
              auto j_idx = (iter->second)*nroots+iroot;
              size_t orbDiff;
              auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
              auto dz = dx * hij;
              auto x = x_vector[j_idx];
              #pragma omp atomic
              z_vector[j_idx] += dz;
              norm += 2.*x*dz;
              if (j_idx % thread_num == thread_id) {
                auto abs_grad = abs(x*xx+z_vector[j_idx]);
                if (abs_grad > max_abs_grad) {
                  max_abs_grad = abs_grad;
                  selected_det = iter->second;
                } 
              }
            }
          }
        }
        for(int ij=0; ij<nclosed*nclosed; ij++) {
          int i=ij/nclosed, j=ij%nclosed;
          if(i<=j) continue;
          int I = closed[i], J = closed[j];
          int X = max(I/2, J/2), Y = min(I/2, J/2);
    
          int pairIndex = X*(X+1)/2+Y;
          size_t start = I%2==J%2 ? I2HB.startingIndicesSameSpin[pairIndex]   : I2HB.startingIndicesOppositeSpin[pairIndex];
          size_t end   = I%2==J%2 ? I2HB.startingIndicesSameSpin[pairIndex+1] : I2HB.startingIndicesOppositeSpin[pairIndex+1];
          float* integrals  = I%2==J%2 ?  I2HB.sameSpinIntegrals : I2HB.oppositeSpinIntegrals;
          short* orbIndices = I%2==J%2 ?  I2HB.sameSpinPairs     : I2HB.oppositeSpinPairs;
          for (size_t index = start; index < end; index++) {
            //if (fabs(integrals[index]) < screen) break;
            int a = 2* orbIndices[2*index] + closed[i]%2, b= 2*orbIndices[2*index+1]+closed[j]%2;
            if(deti.getocc(a) || deti.getocc(b)) continue; 
            Determinant detj = deti;
            detj.setocc(a, true);
            detj.setocc(b, true);
            detj.setocc(I, false);
            detj.setocc(J, false);
            auto iter = det_to_index.find(detj);
            if (iter != det_to_index.end()) {
              auto j_idx = (iter->second)*nroots+iroot;
              size_t orbDiff;
              auto hij = Hij(deti, detj, I1, I2, coreE, orbDiff);
              auto dz = dx * hij;
              auto x = x_vector[j_idx];
              #pragma omp atomic
              z_vector[j_idx] += dz;
              norm += 2.*x*dz;
              if (j_idx % thread_num == thread_id) {
                auto abs_grad = abs(x*xx+z_vector[j_idx]);
                if (abs_grad > max_abs_grad) {
                  max_abs_grad = abs_grad;
                  selected_det = iter->second;
                }
              }
            }
          }
        }
        this_det_idx[thread_id] = selected_det;
        if (iter % 10 == 0) {
          this_det_idx[thread_id] = (iter/10*thread_num)%dets_size;
        }
        //CoordinatePickGcdGradOmp(column, x_vector, z_vector, ene, thread_num, thread_id);
        #pragma omp atomic
        ene[iroot].first += norm;
      }
      #pragma omp barrier
      // now some logical codes, print out information and decide when to exit etc.
      if (iter*thread_num%schd.report_interval < thread_num || iter == num_iter) {
        auto residual = cdfci::compute_residual(x_vector, z_vector, ene, iroot);
        double curr_energy = ene[iroot].first/ene[iroot].second;
        double prev_energy = prev_ene[iroot].first/prev_ene[iroot].second;
        cout << setw(10) << iter * thread_num << setw(20) <<std::setprecision(14) << curr_energy+coreEbkp << setw(20) <<std::setprecision  (14) << prev_energy+coreEbkp;
        cout << std::setw(12) << std::setprecision(4) << scientific << getTime()-start_time << defaultfloat;
        cout << std::setw(12) << std::setprecision(4) << scientific << residual << defaultfloat << endl;
        
        double residual_sum = 0.0;
        if (residual < schd.cdfciTol) {
          break;
        }
        prev_ene = ene;
      }
    }
  }
  coreE = coreEbkp;
  return;
}

