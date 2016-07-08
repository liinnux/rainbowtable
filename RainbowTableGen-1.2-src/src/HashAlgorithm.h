/*
PRTGen - A Parallel implementation of RainbowCrack using MPI.
Copyright (C) 2008 Mike Taber <mstaber@gmail.com>
This source code is based on:
RainbowCrack - a general propose implementation of Philippe
Oechslin's faster time-memory trade-off technique.
Copyright (C) Zhu Shuanglei <shuanglei@hotmail.com>
*/
#ifndef _HASHALGORITHM_H
#define _HASHALGORITHM_H
void HashLM(unsigned char* pPlain, int nPlainLen, unsigned char* pHash);
void HashMD5(unsigned char* pPlain, int nPlainLen, unsigned char* pHash);
void HashSHA1(unsigned char* pPlain, int nPlainLen, unsigned char* pHash);
#endif