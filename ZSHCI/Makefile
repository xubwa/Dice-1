##########################
# User Specific Settings #
##########################

USE_MPI = yes
USE_OMP = no
USE_INTEL = no 
USING_OSX = no

EIGEN=${EIGEN_ROOT}
BOOST=${BOOST_ROOT}

#########################################
# DO NOT EDIT ANYTHING BELOW THIS POINT #
#########################################

git_commit=`git rev-parse HEAD`
git_branch=`git branch | grep "^\*" | sed 's/^..//'`
export VERSION_FLAGS=-Dgit_commit="\"$(git_commit)\"" -Dgit_branch="\"$(git_branch)\""

FLAGS  = -std=c++11 -g -w -O3 -I${EIGEN} -I${BOOST}/include $(VERSION_FLAGS)
DFLAGS = -std=c++11 -g -w -O3 -I${EIGEN} -I${BOOST}/include $(VERSION_FLAGS) -DComplex
LFLAGS = -L${BOOST}/lib -lboost_serialization

ifeq ($(USE_INTEL), yes)
	ifeq ($(USE_OMP), yes)
		FLAGS += -qopenmp
		DFLAGS += -qopenmp
	endif
	ifeq ($(USE_MPI), yes)
		CXX = mpiicpc
		CC = mpiicpc
		LFLAGS += -lboost_mpi
	else
		CXX = icpc
		CC = icpc
		FLAGS += -DSERIAL
		DFLAGS += -DSERIAL
	endif
else
	ifeq ($(USE_OMP), yes)
		FLAGS += -fopenmp
		DFLAGS += -fopenmp
	endif
	ifeq ($(USE_MPI), yes)
		CXX = mpicxx
		CC = mpicxx
		LFLAGS += -lboost_mpi
	else
		CXX = g++
		CC = g++
		FLAGS += -DSERIAL
		DFLAGS += -DSERIAL
	endif
endif

# Add -lrt flag if NOT using Mac OSX
ifeq ($(USING_OSX), no)
	LFLAGS += -lrt
endif

# Host specific configurations.
HOSTNAME := $(shell hostname)
ifneq ($(filter dft node%, $(HOSTNAME)),)
include dft.mk
endif

SRC_qdptsoc    = QDPTSOC.cpp    SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp new_anglib.cpp SOChelper.cpp SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCImakeHamiltonian.cpp
SRC_GTensorFT  = GTensorFT.cpp  SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp new_anglib.cpp SOChelper.cpp SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCImakeHamiltonian.cpp
SRC_GTensorFT2 = GTensorFT2.cpp SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp new_anglib.cpp SOChelper.cpp SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCImakeHamiltonian.cpp

SRC_Dice   = SHCI.cpp SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp                              SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCImakeHamiltonian.cpp SHCIshm.cpp LCC.cpp symmetry.cpp
SRC_ZDice2 = SHCI.cpp SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp SOChelper.cpp new_anglib.cpp SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCImakeHamiltonian.cpp SHCIshm.cpp LCC.cpp symmetry.cpp
SRC_ZSHCI  = SHCI.cpp SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp SOChelper.cpp new_anglib.cpp SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCI4cmakeHamiltonian.cpp SHCIshm.cpp LCC.cpp symmetry.cpp
SRC_ZCDFCI  = SHCI.cpp SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp SOChelper.cpp new_anglib.cpp SHCIgetdeterminants.cpp SHCIsampledeterminants.cpp SHCIrdm.cpp SHCISortMpiUtils.cpp SHCI4cmakeHamiltonian.cpp SHCIshm.cpp LCC.cpp symmetry.cpp cdfci.cpp
SRC_forcyrus    = forCyrus.cpp    SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp Davidson.cpp
SRC_Excitations = Excitations.cpp SHCIbasics.cpp Determinants.cpp integral.cpp input.cpp

OBJ_qdptsoc+=   obj_z/QDPTSOC.o    obj_z/SHCIbasics.o obj_z/Determinants.o obj_z/integral.o obj_z/input.o obj_z/Davidson.o obj_z/new_anglib.o obj_z/SOChelper.o obj_z/SHCIgetdeterminants.o obj_z/SHCIsampledeterminants.o obj_z/SHCIrdm.o obj_z/SHCISortMpiUtils.o obj_z/SHCImakeHamiltonian.o
OBJ_gtensorft+= obj_z/GTensorFT.o  obj_z/SHCIbasics.o obj_z/Determinants.o obj_z/integral.o obj_z/input.o obj_z/Davidson.o obj_z/new_anglib.o obj_z/SOChelper.o obj_z/SHCIgetdeterminants.o obj_z/SHCIsampledeterminants.o obj_z/SHCIrdm.o obj_z/SHCISortMpiUtils.o obj_z/SHCImakeHamiltonian.o
OBJ_gtensorft2+=obj_z/GTensorFT2.o obj_z/SHCIbasics.o obj_z/Determinants.o obj_z/integral.o obj_z/input.o obj_z/Davidson.o obj_z/new_anglib.o obj_z/SOChelper.o obj_z/SHCIgetdeterminants.o obj_z/SHCIsampledeterminants.o obj_z/SHCIrdm.o obj_z/SHCISortMpiUtils.o obj_z/SHCImakeHamiltonian.o

OBJ_Dice  +=obj/SHCI.o   obj/SHCIbasics.o   obj/Determinants.o   obj/integral.o   obj/input.o   obj/Davidson.o                                        obj/SHCIgetdeterminants.o   obj/SHCIsampledeterminants.o   obj/SHCIrdm.o   obj/SHCISortMpiUtils.o   obj/SHCImakeHamiltonian.o   obj/SHCIshm.o obj/LCC.o     obj/symmetry.o
OBJ_ZDice2+=obj_z/SHCI.o obj_z/SHCIbasics.o obj_z/Determinants.o obj_z/integral.o obj_z/input.o obj_z/Davidson.o obj_z/SOChelper.o obj_z/new_anglib.o obj_z/SHCIgetdeterminants.o obj_z/SHCIsampledeterminants.o obj_z/SHCIrdm.o obj_z/SHCISortMpiUtils.o obj_z/SHCImakeHamiltonian.o obj_z/SHCIshm.o obj_z/LCC.o obj_z/symmetry.o
OBJ_ZSHCI +=obj_z/SHCI.o obj_z/SHCIbasics.o obj_z/Determinants.o obj_z/integral.o obj_z/input.o obj_z/Davidson.o obj_z/SOChelper.o obj_z/new_anglib.o obj_z/SHCIgetdeterminants.o obj_z/SHCIsampledeterminants.o obj_z/SHCIrdm.o obj_z/SHCISortMpiUtils.o obj_z/SHCImake4cHamiltonian.o obj_z/SHCIshm.o obj_z/LCC.o obj_z/symmetry.o
OBJ_ZCDFCI +=obj_z/SHCI.o obj_z/SHCIbasics.o obj_z/Determinants.o obj_z/integral.o obj_z/input.o obj_z/Davidson.o obj_z/SOChelper.o obj_z/new_anglib.o obj_z/SHCIgetdeterminants.o obj_z/SHCIsampledeterminants.o obj_z/SHCIrdm.o obj_z/SHCISortMpiUtils.o obj_z/SHCImake4cHamiltonian.o obj_z/SHCIshm.o obj_z/LCC.o obj_z/symmetry.o obj_z/cdfci.o

OBJ_forcyrus+=   obj/forCyrus.o    obj/SHCIbasics.o obj/Determinants.o obj/integral.o obj/input.o obj/Davidson.o obj/SHCIgetdeterminants.o  obj/SHCIsampledeterminants.o obj/SHCIrdm.o obj/SHCISortMpiUtils.o obj/SHCImakeHamiltonian.o
OBJ_Excitations+=obj/Excitations.o obj/SHCIbasics.o obj/Determinants.o obj/integral.o obj/input.o

obj/%.o: %.cpp
	$(CXX) $(FLAGS) $(OPT) -c $< -o $@
obj_z/%.o: %.cpp
	$(CXX) $(DFLAGS) $(OPT) -c $< -o $@


all: ZCDFCI # stats QDPTSOC GTensorFT GTensorFT2

stats: stats.o
	$(CXX) -O3 stats.cpp -o stats
Dice	: $(OBJ_Dice)
	$(CXX)   $(FLAGS) $(OPT) -o  Dice $(OBJ_Dice) $(LFLAGS)
ZDice2	: $(OBJ_ZDice2)
	$(CXX)   $(FLAGS) $(OPT) -o  ZDice2 $(OBJ_ZDice2) $(LFLAGS)
ZSHCI	: $(OBJ_ZSHCI)
	$(CXX)   $(FLAGS) $(OPT) -o  ZDice2 $(OBJ_ZCDFCI) $(LFLAGS)
ZCDFCI	: $(OBJ_ZCDFCI)
	$(CXX)   $(FLAGS) $(OPT) -o  ZCDFCI $(OBJ_ZCDFCI) $(LFLAGS)
forcyrus	: $(OBJ_forcyrus)
	$(CXX)   $(FLAGS) $(OPT) -o  forcyrus $(OBJ_forcyrus) $(LFLAGS)
Excitations	: $(OBJ_Excitations)
	$(CXX)   $(FLAGS) $(OPT) -o  Excitations $(OBJ_Excitations) $(LFLAGS)
SHCI2	: $(OBJ_shci2)
	$(CXX)   $(FLAGS) $(OPT) -o  SHCI2 $(OBJ_shci2) $(LFLAGS)
QDPTSOC	: $(OBJ_qdptsoc)
	$(CXX)   $(DFLAGS) $(OPT) -o  QDPTSOC $(OBJ_qdptsoc) $(LFLAGS)
GTensorFT	: $(OBJ_gtensorft)
	$(CXX)   $(DFLAGS) $(OPT) -o  GTensorFT $(OBJ_gtensorft) $(LFLAGS)
GTensorFT2	: $(OBJ_gtensorft2)
	$(CXX)   $(DFLAGS) $(OPT) -o  GTensorFT2 $(OBJ_gtensorft2) $(LFLAGS)

clean :
	find . -name "*.o"|xargs rm 2>/dev/null;rm -f CIST Dice ZDice2 QDPTSOC GTensorFT forcyrus >/dev/null 2>&1
