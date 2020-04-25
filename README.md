#  NBBS: A Non-Blocking Buddy System
----------------------------------

This repository contains NBBS, a lock-free back-end allocator
that adheres to the buddy system specification.

It also keeps several benchmarks and modules to evaluate NBBS and the
Linux Kernel Buddy System.


----------------------------------
## Allocators

Here, you can find several buddy system allocators.
In particular:

 * 1lvl-nbbs: this is the classical NBBS implementation discussed in several papers [[Cluster'19](https://doi.ieeecomputersociety.org/10.1109/CCGRID.2019.00011), [CCGrid'19](https://doi.ieeecomputersociety.org/10.1109/CCGRID.2019.00011)];
 * 4lvl-nbbs: this is the memory optimized version of our NBBS allocator (16x compress ratio);
 * the spin-locked version of the abovementioned allocators.

----------------------------------

## The Benchmark Suite



----------------------------------
## Contacts

For further information on how use/configure the NBBS allocators, please send an email to:

 ```marotta at diag dot uniroma1 dot it```

----------------------------------
## Authors

Current:

* Romolo Marotta
* Mauro Ianni
* Alessandro Pellegrini
* Francesco Quaglia

Former:

* Andrea Scarselli