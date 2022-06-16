## Guide to phaseless AFQMC calculations

This directory contains two example ph-AFQMC calculations in c_afqmc and h10_afqmc. Preparation for and running of the calculations is handled in a single python script $system_name.py, which is can be executed as

```
python -u $system_name.py > $system_name.out
```

Preparation consists of mean-field calculations, integral generation, and trial state preparation. HCI trial states are calculated using Dice. The script writes input files for Dice and DQMC binaries, executes them, and performs blocking analysis of AFQMC samples to produce an estimate of the energy and stochastic error. The following lines at the beginning of the script should be changed to point to the users' paths:

```
nproc = 8
dice_binary = "/projects/anma2640/newDice/Dice/Dice"
vmc_root = "/projects/anma2640/VMC/master/VMC"
```

vmc_root/QMCUtils.py contains tools used for setting up QMC calculations and needs to be in the PYTHONPATH. Details of Dice input options specified in the script can be found in the [Dice documentaion](https://sanshar.github.io/Dice/). 

AFQMC calculations can be roughly divided into two parts: propagation and measurement. Trial states affect both these parts; and their accuracy dictates the systematic error (phaseless error) and efficiency (statistical noise) of the AFQMC calculation. Observables, energies in the the examples here, are only measured periodically after several propagation steps because the samples are serially correlated and energy measurements are expensive. Following is a rough description of the relevant AFQMC input options:

1. Wave function options:
  * "left": trial state used to measure energy and for importance sampling and phasless constraint, typically "rhf", "uhf" or "multislater" (any CI state)
  * "right": initial state which affects equilibration time and can serve to impose certain symmetries, typically "rhf" or "uhf"
  * "dets": name of the file contating HCI dets and coefficients if using this trial, default is "dets.bin", should contain dets sorted by coefficient magnitudes
  * "ndets": number of leading dets to pick from the dets file

2. Sampling options:
  * "seed": random number seed to be able to reproduce results
  * "dt": size of a single propagation time step, dictates Trotter error, the default conservative value of 0.005 should be good enough for most applications
  * "nsteps": number of propagation steps after which a measurement is performed (number of steps in a propagation batch, ~50), determines correlation length
  * "nwalk": number of walkers on each process (~20-50), there should be enough of these in total (nwalk * nproc) to mitigate the population control bias 
  * "stochasticIter": number of energy measurements performed for each walker (number of propagation batches, ~500-1000, one measurement at the end of a batch), there should be enough of these to allow a blocking analysis to estimate stocahstic error

All these options can be specified in the main script as arguments for this function:

```
QMCUtils.write_afqmc_input(seed = 4321, left="rhf", right="rhf", nwalk=50, stochasticIter=500, choleskyThreshold=1.e-3, fname="afqmc_rhf.json")
```

This creates an input json file for the DQMC binary. 

The three options "nsteps", "nwalk", and "stochasticIter" determine the number of samples collected (= nproc * nwalk * stochasticIter) and the correlation length (inversely propportional to nsteps). Thus the noise and the cost of the calculation is dictated by these options. They need to be adjusted depending on the system and while some heuristics exist, it is mostly guesswork. I usually use the values noted above; and increase the number of processes for bigger and more difficult systems. 


## Miscellaneous 
1. One useful thing to keep in mind when choosing sampling options is that since the noise in QMC scales as 1 / sqrt(number of samples), reducing the noise by a factor of two requires roughly four times as many samples!
2. For HCI trial states, using more dets leads to less noisy and usually less biased estimators but at a higher cost. We find it useful to pick determinants from a moderately sized active space from which converged energies can be obtained with less than ~100k dets.
3. Blocking analysis is performed with the script vmc_root/scripts/blocking.py to estimate stochastic errors from the serially correlated samples. To estimate the stochastic error, one needs to look for an intermediate plateau in the error as a function of the block size. Consider the following example:

    ```
    reading samples from samples_rhf.dat, ignoring first 50
    mean: -5.382689152884524
    blocked statistics:
    block size    # of blocks        mean                error
         1            450       -5.38268915e+00       5.397366e-04
         2            225       -5.38268915e+00       7.067836e-04
         5             90       -5.38268915e+00       1.029813e-03
        10             45       -5.38268915e+00       1.039668e-03
        20             22       -5.38266672e+00       1.036828e-03
        50              9       -5.38268915e+00       1.512644e-03
        70              6       -5.38272344e+00       1.395108e-03
       100              4       -5.38261626e+00       1.733287e-03
       200              2       -5.38261626e+00       3.212095e-03
    ```

The first 50 samples (batches) were discarded as equilibration steps. The error column shows increasing values of the sample variance as block size is increased indicating serial correlation. It is clear that there is a plateau in error values around 1.e-3 mH for block sizes of 5-20 after which the errors seem to diverge because of small number of blocks. So the energy in this example can be reported as -5.383(1) H.
4. All parallelization is done through MPI and thereading should be disabled so as not to interfere with MPI. The parallelization is not perfect because there needs to be communication for population control.
5. I use DQMC and AFQMC interchangeably in many places and not in others. For example, the dqmc examples in this directory refer to free projection, but the binary DQMC is used for both free projection AFQMC and phaseless AFQMC calculations.
