/*
PRTGen - A Parallel implementation of RainbowCrack using MPI.
Copyright (C) 2008 Mike Taber <mstaber@gmail.com>
*/
#include "Benchmark.h"
Benchmark::Benchmark(void)
{
processID=0;
hostname="";
speed=0;
}
Benchmark::~Benchmark(void)
{
}
Benchmark& Benchmark::operator=(const Benchmark &rhs)
{
this->processID = rhs.processID;
this->hostname = rhs.hostname;
this->speed = rhs.speed;
this->waitingTimeStart = rhs.waitingTimeStart;
this->dWorkingTime = rhs.dWorkingTime;
this->dWaitingTime = rhs.dWaitingTime;
this->dIdleTime = rhs.dIdleTime;
return *this;
}
// equality is based on a process ID and a hostname. That's it.
int Benchmark::operator==(const Benchmark &rhs) const
{
if( this->processID != rhs.processID) return 0;
if( this->hostname.compare(rhs.hostname) != 0 ) return 0;
return 1;
}
// This function is required for built-in STL list functions like sort
int Benchmark::operator<(const Benchmark &rhs) const
{
// sort by speed
if( this->speed < rhs.speed ) return 1;
return 0;
}
ostream &operator<<(ostream &output, const Benchmark &b)
{
output << b.processID << ' ' << b.hostname.c_str() << ' ' <<
b.speed << b.dWorkingTime << b.dWaitingTime << b.dIdleTime << endl;
return output;
}