# CSED211-Computer-SW-Systems

2023 Fall Semester's CS:APP Lectures (and my Lab Solutions)

## Lab1 , Lab2 (Datalab)

Datalab: Using tricky, sometimes evil bitwise operations.

Lab1 handles only integers / Lab2 handles floating point operations.


## Lab3 (Bomblab)

Defuse the bomb, with baby-baby-baby reverse engineering. ~~Some of us might use IDA or Ghidra, not only gdb!!~~

For fun, I made the project named [qwoqLab](https://github.com/Aplace0927/qwoqLab), which is FULLY automated bomblab solution finder with symbolic executioning tools.

## Lab4 (Attacklab)

Simple buffer overflow and ROP attacks. ~~And shadowstack is here.~~

Tooling is everything, I used `ROPgadget`, `pwntools` to make exploit code more efficient and clearer.

## Lab6 (Cachelab)

Make your own cache simulator, and construct efficient(cache-friendly) matrix transpose code.

**NOTE: IF YOU ARE WORKING ON WSL, DO NOT PUT CACHELAB SIMULATOR ON MOUNTED WINDOWS DIRECTORIES**

### Cachelab Result (80/80)
```text
Part A: Testing cache simulator
Running ./test-csim
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27


Part B: Testing transpose function
Running ./test-trans -M 32 -N 32
Running ./test-trans -M 64 -N 64
Running ./test-trans -M 61 -N 67

Cache Lab summary:
                        Points   Max pts      Misses
Csim correctness          27.0        27
Trans perf 32x32           8.0         8         288
Trans perf 64x64           8.0         8        1180
Trans perf 61x67          10.0        10        1848
          Total points    53.0        53

```


## Lab7 (Shelllab)

Implement `tsh` - tiny shell to understand process lifecycles and signaling (handling, blocking...)

## Lab8 (Malloclab)

Write explicit memory allocation simulator. I referenced glibc malloc.

### Malloclab Result (97/100)
```text
Using default tracefiles in ./traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000321 17749
 1       yes   99%    4805  0.000244 19733
 2       yes   97%   12000  0.000446 26918
 3       yes   97%    8000  0.000323 24760
 4       yes   90%   24000  0.001409 17032
 5       yes   90%   16000  0.000635 25205
 6       yes   99%    5848  0.000327 17867
 7       yes   99%    5032  0.000252 19976
 8       yes   96%   14400  0.000602 23924
 9       yes   96%   14400  0.000605 23817
10       yes   99%    6648  0.000376 17690
11       yes   99%    5683  0.000289 19671
12       yes   99%    5380  0.000293 18337
13       yes   99%    4537  0.000230 19726
14       yes   96%    4800  0.000646  7429
15       yes   96%    4800  0.000855  5613
16       yes   95%    4800  0.000642  7480
17       yes   95%    4800  0.000654  7344
18       yes   99%   14401  0.000283 50887
19       yes   99%   14401  0.000276 52215
20       yes   85%   14401  0.000239 60356
21       yes   85%   14401  0.000237 60661
22       yes   95%      12  0.000001 20000
23       yes   95%      12  0.000001 20000
24       yes   88%      12  0.000001 17143
25       yes   88%      12  0.000001 15000
Total          95%  209279  0.010185 20548

Perf index = 57 (util) + 40 (thru) = 97/100
```
