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
#ifndef OPTIMIZERSGD_HEADER_H
#define OPTIMIZERSGD_HEADER_H
#include <Eigen/Dense>
#include <boost/serialization/serialization.hpp>
#include "iowrapper.h"
#include "global.h"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#ifndef SERIAL
#include "mpi.h"
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi.hpp>
#endif

using namespace Eigen;
using namespace boost;

class SGD
{
  private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)
    {
      ar & iter;        
    }

  public:
    double stepsize;
    double momentum;

    int maxIter;
    int iter;

    SGD(double pstepsize=0.001, double pmomentum=0.,
        int pmaxIter=1000) : stepsize(pstepsize), momentum(pmomentum), maxIter(pmaxIter)
    {
        iter = 0;
    }

    void write(VectorXd& vars)
    {
        if (commrank == 0)
        {
            char file[5000];
            sprintf(file, "sgd.bkp");
            std::ofstream ofs(file, std::ios::binary);
            boost::archive::binary_oarchive save(ofs);
            save << *this;
            save << vars;
            ofs.close();
        }
    }

    void read(VectorXd& vars)
    {
        if (commrank == 0)
        {
            char file[5000];
            sprintf(file, "sgd.bkp");
            std::ifstream ifs(file, std::ios::binary);
            boost::archive::binary_iarchive load(ifs);
            load >> *this;
            load >> vars;
            ifs.close();
        }
    }

   template<typename Function>
    void optimize(VectorXd &vars, Function& getGradient, bool restart)
    {
        if (restart)
        {
            if (commrank == 0)
                read(vars);
#ifndef SERIAL
	    boost::mpi::communicator world;
	    boost::mpi::broadcast(world, *this, 0);
	    boost::mpi::broadcast(world, vars, 0);
#endif
        }

        VectorXd grad = VectorXd::Zero(vars.rows());
        VectorXd update = VectorXd::Zero(vars.rows());
        double E0 = 0., EOld = 0.; bool skip = 0;

        while (iter < maxIter)
        {
            double stddev = 0.0, rt = 1.0;
            //if (skip == 0) EOld = E0;
            VectorXd varsOld = vars;
            getGradient(vars, grad, E0, stddev, rt);
            //if (E0 - EOld > 10 * stddev) {skip = 1; continue;}
            //else skip = 0;
            if (commrank == 0 && schd.printGrad) {
              VectorXd::Index maxInd;
              double maxVal = grad.array().abs().matrix().maxCoeff(&maxInd);
              cout << "gradMax val: " << maxVal << ", maxInd: " << maxInd << endl; 
              if (schd.printLevel > 15) cout << "totalGrad\n" << grad << endl;
            }
            write(vars);

            if (commrank == 0)
            {
               update = momentum * update + stepsize * grad;
               vars -= update;
            }

#ifndef SERIAL
            MPI_Bcast(&vars[0], vars.rows(), MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

            if (commrank == 0)
                std::cout << format("%5i %14.8f (%8.2e) %14.8f %8.1f %10i %8.2f\n") % iter % E0 % stddev % (grad.norm()) % (rt) % (schd.stochasticIter) % ((getTime() - startofCalc));
            iter++;
        }
    }
};
#endif
