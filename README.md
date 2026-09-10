# Task DAG Engine

*Motivation: I wanted to become familiar with modern C++ development and explore what the standard library has to offer, particularly for low-latency optimization*

Thread-pool executor for a task DAG. nodes are callables, edges are dependencies; workers continuously pull ready nodes to run

Features explored:
- Threading: `jthread`, `mutex`, `unique_lock`, `atomic`, `binary_semaphore`, `counting_semaphore`, `hardware_concurrency`
- Keeping the critical section small: unlock the queue mutex before running the task; semaphores instead of spinning / condvar for idle workers and "all done"
- Scheduling: `priority_queue` + cached `subgraph_size` so larger remaining subgraphs run first (critical-path-ish heuristic)
- DAG representation: pointer graph, `unordered_set` successors, atomic in-degrees so a graph can be reset and reused
- Ranges / views: `ranges::for_each`, `ranges::any_of`, `views::transform`, `ranges::to`
- Callables: `std::function<void()>`, templated ctors w/ `&&`, `std::forward`, `reference_wrapper`
- Memory / ownership: `unique_ptr` for the worker array and bench graphs; nodes stay address-stable so successor pointers don't dangle
- Atomics as a memory model: default `seq_cst` dep counters, `load` on reset; still want to play w/ weaker orders later
- Output: `osyncstream` so workers don't interleave I/O; `std::format` for the bench tables
- Performance timing: `chrono::steady_clock`, warmup + batched samples, median / p95 / stdev of `compute_DAG` + `wait` on many distinct DAG shapes
- Many other quirks that are available from C++23

`make` runs `ThreadPoolExe::check_performance()`, which times `ThreadPoolExe` on DAGs of shapes: chain / star / binary tree / funnel / grid / layered / par_join / random. A simple usage demo can be found in `example.cpp`.

Example benchmark output:

```
ThreadPoolExe::check_performance
  times compute_DAG + wait on the same graph (reset between runs)
  each node does a short busy-loop so width can actually use the workers
  clock: steady_clock | samples: 11 batched | prefer -O2
Will run on 8 hardware threads

  graph                                     n    edges   srcs       median          min          p95    batch    ns/node
  chain n=200                             200      199      1    676.34 us    664.82 us    707.56 us       12     3381.7
  chain n=2000                           2000     1999      1     6.888 ms     6.638 ms     7.200 ms        1     3443.8
  chain n=8000                           8000     7999      1    27.338 ms    26.562 ms    28.509 ms        1     3417.3
  star n=200                              200      199      1    272.75 us    262.01 us    281.56 us       30     1363.7
  star n=2000                            2000     1999      1     2.723 ms     2.217 ms     3.103 ms        3     1361.6
  star n=15000                          15000    14999      1    20.476 ms    17.936 ms    23.060 ms        1     1365.1
  bin_tree n=511                          511      510      1    676.55 us    625.83 us    778.08 us       12     1324.0
  bin_tree n=4095                        4095     4094      1     4.951 ms     4.832 ms     5.248 ms        1     1209.1
  bin_tree n=8191                        8191     8190      1    10.076 ms     9.898 ms    10.881 ms        1     1230.1
  funnel n=200                            200      199    100    256.30 us    243.09 us    276.92 us       24     1281.5
  funnel n=2000                          2000     1999   1000     2.560 ms     2.435 ms     2.919 ms        2     1279.8
  funnel n=8000                          8000     7999   4000    11.349 ms    11.052 ms    11.481 ms        1     1418.7
  grid 16x16                              256      480      1    354.28 us    344.06 us    391.45 us       17     1383.9
  grid 40x40                             1600     3120      1     1.927 ms     1.871 ms     2.025 ms        4     1204.2
  grid 80x80                             6400    12640      1     7.967 ms     7.666 ms     8.166 ms        1     1244.9
  layered 8x40 d=3                        320      840     49    418.27 us    399.65 us    427.79 us       17     1307.1
  layered 20x80 d=3                      1600     4560    162     2.140 ms     2.016 ms     2.252 ms        3     1337.6
  layered 30x100 d=4                     3000    11600    150     4.349 ms     3.975 ms     4.378 ms        1     1449.7
  par_join 8x40                           322      328      1    401.87 us    391.68 us    412.73 us       18     1248.1
  par_join 20x80                         1602     1620      1     1.943 ms     1.894 ms     2.045 ms        4     1212.8
  par_join 40x150                        6002     6040      1     7.764 ms     7.497 ms     7.941 ms        1     1293.5
  random n=500 out=3 seed=42              500     1494    121    711.19 us    693.85 us    721.76 us       11     1422.4
  random n=4000 out=3 seed=42            4000    11994    997     5.984 ms     5.713 ms     6.451 ms        1     1496.1
  random n=4000 out=3 seed=1337          4000    11994   1012     5.910 ms     5.777 ms     6.430 ms        1     1477.5
  random n=4000 out=3 seed=20260908      4000    11994   1005     5.822 ms     5.660 ms     6.457 ms        1     1455.5
  random n=10000 out=3 seed=42          10000    29994   2477    15.837 ms    15.644 ms    15.989 ms        1     1583.7
  random n=10000 out=8 seed=42          10000    79964   1089    18.908 ms    17.902 ms    23.628 ms        1     1890.8

done.
```