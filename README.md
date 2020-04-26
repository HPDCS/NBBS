#  NBBS: A Non-Blocking Buddy System
----------------------------------

This repository contains NBBS, a lock-free back-end allocator
that adheres to the buddy system specification.

It also keeps several benchmarks and modules to evaluate NBBS and the
Linux Kernel Buddy System.

All the code targets x86_64 architecture.

----------------------------------
## Allocators

Here, you can find several buddy system allocators.
In particular:

 * 1lvl-nb: this is the classical NBBS implementation discussed in several papers [[Cluster'18](https://doi.org/10.1109/CLUSTER.2018.00034), [CCGrid'19](https://doi.ieeecomputersociety.org/10.1109/CCGRID.2019.00011)];
 * 4lvl-nb: this is the memory optimized version of our NBBS allocator (16x compress ratio);
 * the spin-locked version of the above-mentioned allocators (1lvl-sl and 4lvl-sl).

----------------------------------

## The Benchmark Suite

We provide several benchmarks to evaluate NBBS and other back-end allocators.
The benchmarks are built to run also on kernel side, so you can even evaluate the Linux-Kernel buddy system.

### Description

 * [Linux scalability](http://www.citi.umich.edu/techreports/reports/citi-tr-00-5.pdf):
   each thread makes a burst of allocations followed by a burst of memory release operations.
 * [Thread test](http://doi.acm.org/10.1145/378993.379232):
   it operates like linux scalability, but the length of bursts is divided by the number of threads.
 * [Costant occupancy](https://doi.ieeecomputersociety.org/10.1109/CCGRID.2019.00011):
   each thread pre-allocates blocks of different order and makes a burst of allocations followed by a burst of memory release operations.
 * Cached allocation: each thread repeatedly allocates and releases an individual memory buffer.

### Instructions

* Compile just typing `make`
* In each benchmark folder you will find one executable for each allocator. Run them by typing
`
./TB_<bench_name> <num_of_threads> <mem_size>
`



----------------------------------
## Contacts

For further information about NBBS allocators, please send an email to:

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