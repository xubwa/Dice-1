<div><img src="https://github.com/sanshar/Dice/blob/master/docs/images/dice_lateral.png" height="360px"/></div>

*Dice* implements the semistochastic heat bath configuration interaction (SHCI) algorithm for *ab initio* Hamiltonians of quantum chemical systems.

Unlike full configuration interaction (FCI), SHCI can be used to treat active spaces containing 30 to 100 orbitals.
SHCI is able to accomplish this by taking advantage of the fact that although the full Hilbert space may be enormous,
only a small fraction of the determinants in the space have appreciable coefficients.

Compared to other methods in its class, SHCI is often not only orders of magnitude faster,
it also does not suffer from serious memory bottlenecks that plagues these methods.
The resulting algorithm as implemented in *Dice* allows us to treat difficult benchmark systems
such as the Chromium dimer and Mn-Salen (a challenging bioinorganic cluster) at a cost that is often
an order of magnitude faster than density matrix renormalization group (DMRG) or full configuration interaction quantum Monte Carlo (FCIQMC).

Thus if you are interested in performing multireference calculations with active space containing several tens to hundreds of orbitals,
*Dice* might be an ideal choice for you.


* *Dice* is available with the [PySCF](https://github.com/sunqm/pyscf/blob/master/README.md) package.

* The latest version of *Dice* is also downloadable as a tar archive: [Dice.tar.gz](images/Dice.tar.gz)

Prerequisites
------------

*Dice* requires:

* [Boost](http://www.boost.org/) (when compiling the Boost library make sure that you use the same compiler as you do for *Dice*)

An example of download and compilation commands for the `NN` version of Boost can be:

```
  wget https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_NN_0.tar.gz
  tar -xf boost_1_NN_0.tar.gz
  cd boost_1_NN_0
  ./bootstrap.sh
  echo "using mpi ;" >> project-config.jam
  ./b2 -j6 --target=shared,static
```


* [Eigen](http://eigen.tuxfamily.org/dox/) (Eigen consists of header files and does not have to be compiled but can be installed)

One way of getting and installing the Eigen package is:

```
  hg clone https://bitbucket.org/eigen/eigen/
  cd eigen
  mkdir build_dir
  cd build_dir
  cmake ..
  sudo make install
```

* About compiler requirements:
    - GNU: g++ 4.8 or newer
    - Intel: icpc 14.0.1 or newer
    - In any case: the C++0x/C++11 standards must be supported.


Compilation
-------

Edit the `Makefile` in the main directory and change the paths to your Eigen and Boost libraries.
The user can choose whether to use gcc or intel by setting the `USE_INTEL` variable accordingly,
and whether or not to compile with MPI by setting the `USE_MPI` variable.
All the lines in the `Makefile` that normally need to be edited are shown below:

```
  USE_MPI = yes
  USE_INTEL = yes
  EIGEN=/path_to/eigen
  BOOST=/path_to/boost_1_NN_0
```


Testing
-------

Upon successful compilation, one can test the code using the `runTests.sh` script in `/path_to/Dice/tests/`
(before running this script, edit the `MPICOMMAND` variable to the appropriate number of processors you wish to run the tests onto):

```
  cd /path_to/Dice/tests/
  ./runTests.sh
```


  If your system has limited memory or slow processing power, you may wish to comment out the tests for Mn(salen) in the `runTests.sh`
  script because they require a large amount of processing power and memory.

# QMC methods for *ab initio* electronic structure theory

This repository contains code for performing VMC, GFMC, DMC, FCIQMC, stochastic MRCI and SC-NEVPT2, and AFQMC calculations with a focus on *ab initio* systems. Many parts of the code are under development, and documentation and examples for various methods will be added soon! 

## Compiling the DQMC binary 
The DQMC (determinantal QMC) part of the code can perform free projection and phaseless AFQMC (auxiliary field QMC). Compilation is similar to the [Dice](https://github.com/sanshar/Dice/) code. It has the following dependencies:
 
1. Boost (known to work with versions up to 1.70)
2. HDF5

The header-only [Eigen linear algebra library](https://gitlab.com/libeigen/eigen) is included in this repository. Intel or GCC (recommended) compilers can be used. The compiler as well as paths for dependencies are specified at the top of the Makefile:
```
USE_MPI = yes
USE_INTEL = no
ONLY_DQMC = no
HAS_AVX2 = yes

BOOST=${BOOST_ROOT}
HDF5=${CURC_HDF5_ROOT}
```

Note that the Boost libraries (mpi and serialization) have to be compiled with the same compiler used for compiling DQMC. More information about compiling Boost can be found in the [Dice documentation](https://sanshar.github.io/Dice/). Since we use MPI for parallelization, MPI compilers should be used (we have tested with Intel MPI). HAS_AVX2 should be set to no if your processor does not support avx2. After modifying the Makefile, DQMC can be compiled using
```
make bin/DQMC -j
```
Tests can be run using the runDQMC.sh script in the "tests" directory. Examples of phaseless AFQMC calculations are in the "examples" directory along with output files. The python scripts used in these examples require python 3.6 or newer. They also require pyscf and pandas packages. 


## Compiling other parts of the code

Other parts of the code have the following additional dependencies:

1. Libigl
2. Sparsehash
3. More Boost libraries (program_options, system, filesystem) 

Everything on the master branch can be compiled with
```
make -j
```
to generate various binaries for the other methods mentioned above. Tests for most of the code are in the "tests" directory and can be run with the runTests.sh script.
