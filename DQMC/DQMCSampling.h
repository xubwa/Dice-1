#ifndef DQMCSampling_HEADER_H
#define DQMCSampling_HEADER_H
#include <Eigen/Dense>
#include <utility>
#include <vector>

// calculates energy of the imaginary time propagated wave function using direct sampling of exponentials
void calcEnergyDirect(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);
void calcEnergyDirectVartiational(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);

// calculates energy of the imaginary time propagated wave function using direct sampling of exponentials
void calcEnergyDirectMultiSlater(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);

// calculates mixed energy estimator of the imaginary time propagated wave function
// w jastrow
void calcEnergyJastrowDirect(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);

void calcEnergyCCSDDirect(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);
void calcEnergyCCSDDirectVariational(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);
void calcEnergyCCSDMultiSlaterDirect(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);

// calculates variational energy estimator of the imaginary time propagated wave function
// w jastrow
void calcEnergyJastrowDirectVariational(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, vector<Eigen::MatrixXd>& chol);

void calcEnergyDirectGHF(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);

void findDtCorrelatedSamplingGHF(double enuc, Eigen::MatrixXd& h1, Eigen::MatrixXd& h1Mod, std::vector<Eigen::MatrixXd>& chol);
#endif
