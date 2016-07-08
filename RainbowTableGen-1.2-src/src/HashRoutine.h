/*
PRTGen - A Parallel implementation of RainbowCrack using MPI.
Copyright (C) 2008 Mike Taber <mstaber@gmail.com>
This source code is based on:
RainbowCrack - a general propose implementation of Philippe
Oechslin's faster time-memory trade-off technique.
Copyright (C) Zhu Shuanglei <shuanglei@hotmail.com>
*/
#ifndef _HASHROUTINE_H
#define _HASHROUTINE_H
#include <string>
#include <vector>
using namespace std;
typedef void (*HASHROUTINE)(unsigned char* pPlain, int nPlainLen,
unsigned char* pHash);
class CHashRoutine
{
public:
CHashRoutine();
virtual ~CHashRoutine();
private:
vector<string> vHashRoutineName;
vector<HASHROUTINE> vHashRoutine;
vector<int> vHashLen;
void AddHashRoutine(string sHashRoutineName, HASHROUTINE pHashRoutine, int nHashLen);
public:
string GetAllHashRoutineName();
void GetHashRoutine(string sHashRoutineName, HASHROUTINE& pHashRoutine, int& nHashLen);
};
#endif