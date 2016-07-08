/*
PRTGen - A Parallel implementation of RainbowCrack using MPI.
Copyright (C) 2008 Mike Taber <mstaber@gmail.com>
*/
#pragma once
#include <iostream>
#include <list>
#include <time.h>
using namespace std;
class Benchmark
{
friend ostream &operator<<(ostream &, const Benchmark &);
public:
Benchmark(void);
~Benchmark(void);
Benchmark &operator=(const Benchmark &rhs);
int operator==(const Benchmark &rhs) const;
int operator<(const Benchmark &rhs) const;
public:
int processID;
string hostname;
long speed;
clock_t waitingTimeStart; // used to keep track of when this process first started waiting for other processes to complete
double dWorkingTime; // time spent doing "real work"
double dWaitingTime; // time spent not doing "real work"
double dIdleTime; // time the process spent waiting for other nodes to finish their work
// dWorkingTime + dWaitingTime + dIdleTime = dTotalTime
};