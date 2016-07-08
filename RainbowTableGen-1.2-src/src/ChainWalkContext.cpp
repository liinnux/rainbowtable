/*
PRTGen - A Parallel implementation of RainbowCrack using MPI.
Copyright (C) 2008 Mike Taber <mstaber@gmail.com>
This source code is based on:
RainbowCrack - a general propose implementation of Philippe
Oechslin's faster time-memory trade-off technique.
Copyright (C) Zhu Shuanglei <shuanglei@hotmail.com>
*/
#ifdef _WIN32
#pragma warning(disable : 4786)
#endif
#include "ChainWalkContext.h"
#include <ctype.h>
#include <openssl/rand.h>
#ifdef _WIN32
#pragma comment(lib, "libeay32.lib")
#endif
//////////////////////////////////////////////////////////////////////
string CChainWalkContext::m_sHashRoutineName;
HASHROUTINE CChainWalkContext::m_pHashRoutine;
int CChainWalkContext::m_nHashLen;
unsigned char CChainWalkContext::m_PlainCharset[256];
int CChainWalkContext::m_nPlainCharsetLen;
int CChainWalkContext::m_nPlainLenMin;
int CChainWalkContext::m_nPlainLenMax;
string CChainWalkContext::m_sPlainCharsetName;
string CChainWalkContext::m_sPlainCharsetContent;
uint64 CChainWalkContext::m_nPlainSpaceUpToX[MAX_PLAIN_LEN + 1];
uint64 CChainWalkContext::m_nPlainSpaceTotal;
int CChainWalkContext::m_nRainbowTableIndex;
uint64 CChainWalkContext::m_nReduceOffset;
//////////////////////////////////////////////////////////////////////
CChainWalkContext::CChainWalkContext()
{
}
CChainWalkContext::~CChainWalkContext()
{
}
bool CChainWalkContext::LoadCharset(string exePath, string sName)
{
if (sName == "byte")
{int i;
for (i = 0x00; i <= 0xff; i++)
m_PlainCharset[i] = i;
m_nPlainCharsetLen = 256;
m_sPlainCharsetName = sName;
m_sPlainCharsetContent = "0x00, 0x01, ... 0xff";
return true;
}
vector<string> vLine;
string filePath = exePath + "\\charset.txt";
if (ReadLinesFromFile(filePath.c_str(), vLine))
{
int i;
for (i = 0; i < (int)vLine.size(); i++)
{
// Filter comment lines
if (vLine[i][0] == '#')
continue;
vector<string> vPart;
if (SeperateString(vLine[i], "=", vPart))
{
// sCharsetName
string sCharsetName = TrimString(vPart[0]);
if (sCharsetName == "")
continue;
// sCharsetName charset check
// Valid characters in the sCharsetName are alpha-numeric, and dashes ('-').
// Anything else that appears generates an error
bool fCharsetNameCheckPass = true;
int j;
for (j = 0; j < (int)sCharsetName.size(); j++)
{
if ( !isalpha(sCharsetName[j])
&& !isdigit(sCharsetName[j])
&& (sCharsetName[j] != '-'))
{
fCharsetNameCheckPass = false;
break;
}
}
if (!fCharsetNameCheckPass)
{
printf("invalid charset name %s in charset configuration file\n", sCharsetName.c_str());
continue;
}
// sCharsetContent
string sCharsetContent = TrimString(vPart[1]);
if (sCharsetContent == "" || sCharsetContent =="[]")
{// skip empty character sets
continue;
}
if (sCharsetContent[0] != '[' ||sCharsetContent[sCharsetContent.size() - 1] != ']')
{
// skip character sets that don't start and end with square brackets
printf("invalid charset content %s in charset configuration file\n", sCharsetContent.c_str());
continue;
}
// set the contents of the characterset
sCharsetContent = sCharsetContent.substr(1,sCharsetContent.size() - 2);
if (sCharsetContent.size() > 256)
{
// charactersets are not allowed to be more than 256 bytes long.
// skip these if they appear as well
printf("charset content %s too long\n",sCharsetContent.c_str());
continue;
}
// Is it the wanted charset?
if (sCharsetName == sName)
{
m_nPlainCharsetLen =(int)sCharsetContent.size();
memcpy(m_PlainCharset,sCharsetContent.c_str(), m_nPlainCharsetLen);
m_sPlainCharsetName = sCharsetName;
m_sPlainCharsetContent = sCharsetContent;
return true;
}
}
}
printf("charset %s not found in charset.txt\n",sName.c_str());
}
else
{
printf("can't open charset configuration file\n\t%s\n",filePath.c_str());
}
return false;
}
//////////////////////////////////////////////////////////////////////
bool CChainWalkContext::SetHashRoutine(string sHashRoutineName)
{
CHashRoutine hr;
hr.GetHashRoutine(sHashRoutineName, m_pHashRoutine, m_nHashLen);
if (m_pHashRoutine != NULL)
{
m_sHashRoutineName = sHashRoutineName;
return true;
}
else
return false;
}
bool CChainWalkContext::SetPlainCharset(string exePath, string sCharsetName, int nPlainLenMin, int nPlainLenMax)
{
// m_PlainCharset, m_nPlainCharsetLen, m_sPlainCharsetName,m_sPlainCharsetContent
if (!LoadCharset(exePath, sCharsetName))
return false;
// m_nPlainLenMin, m_nPlainLenMax
// perform error checking on the minimum and maximum plaintext length
if (nPlainLenMin < 1 || nPlainLenMax > MAX_PLAIN_LEN || nPlainLenMin > nPlainLenMax)
{
printf("invalid plaintext length range: %d - %d\n",nPlainLenMin, nPlainLenMax);
return false;
}
// set the class static variables
m_nPlainLenMin = nPlainLenMin;
m_nPlainLenMax = nPlainLenMax;
// calculate the key space for this run, based on the size of the character set, and the plaintext range.
// each entry in the array "m_nPlainSpaceUpToX" stores the key space calculated so far for each plaintext length,
// and all previous plaintext lengths.
// The purpose of this is to be able to help map an index number to a specific plaintext value
// the first element in m_nPlainSpaceUpToX is always ZERO
m_nPlainSpaceUpToX[0] = 0;
uint64 nTemp = 1;
int i;
for (i = 1; i <= m_nPlainLenMax; i++)
{
nTemp *= m_nPlainCharsetLen;
if (i < m_nPlainLenMin)
m_nPlainSpaceUpToX[i] = 0;
else
m_nPlainSpaceUpToX[i] = m_nPlainSpaceUpToX[i - 1] + nTemp;
}
// m_nPlainSpaceTotal
m_nPlainSpaceTotal = m_nPlainSpaceUpToX[m_nPlainLenMax];
return true;
}
bool CChainWalkContext::SetRainbowTableIndex(int nRainbowTableIndex)
{
// the RainbowTableIndex must be a number greater than or equal to zero
if (nRainbowTableIndex < 0)
return false;
m_nRainbowTableIndex = nRainbowTableIndex;
// m_nReduceOffset is a mechanism for helping to ensure that the reduction function used in each rainbow table file is unique,
// even for the same position, or at least it is extremely likely to be unique
m_nReduceOffset = 65536 * nRainbowTableIndex;
return true;
}
bool CChainWalkContext::SetupWithPathName(string exePath, string
sPathName, int& nRainbowChainLen, int& nRainbowChainCount)
{
// something like lm_alpha#1-7_0_100x16_test.rt
#ifdef _WIN32
int nIndex = (int)sPathName.find_last_of('\\');
#else
int nIndex = sPathName.find_last_of('/');
#endif
if (nIndex != -1)
sPathName = sPathName.substr(nIndex + 1);
if (sPathName.size() < 3)
{
printf("%s is not a rainbow table\n", sPathName.c_str());
return false;
}
if (sPathName.substr(sPathName.size() - 3) != ".rt")
{
printf("%s is not a rainbow table\n", sPathName.c_str());
return false;
}
// Parse
vector<string> vPart;
if (!SeperateString(sPathName, "___x_", vPart))
{
printf("filename %s not identified\n", sPathName.c_str());
return false;
}
string sHashRoutineName = vPart[0];
int nRainbowTableIndex = atoi(vPart[2].c_str());
nRainbowChainLen = atoi(vPart[3].c_str());
nRainbowChainCount = atoi(vPart[4].c_str());
// Parse charset definition
string sCharsetDefinition = vPart[1];
string sCharsetName;
int nPlainLenMin, nPlainLenMax;
if (sCharsetDefinition.find('#') == -1) // For backward compatibility, "#1-7" is implied
{
sCharsetName = sCharsetDefinition;
nPlainLenMin = 1;
nPlainLenMax = 7;
}
else
{
vector<string> vCharsetDefinitionPart;
if (!SeperateString(sCharsetDefinition, "#-",vCharsetDefinitionPart))
{
printf("filename %s not identified\n",sPathName.c_str());
return false;
}
else
{
sCharsetName = vCharsetDefinitionPart[0];
nPlainLenMin = atoi(vCharsetDefinitionPart[1].c_str());
nPlainLenMax = atoi(vCharsetDefinitionPart[2].c_str());
}
}
// Setup
if (!SetHashRoutine(sHashRoutineName))
{
printf("hash routine %s not supported\n",sHashRoutineName.c_str());
return false;
}
if (!SetPlainCharset(exePath, sCharsetName, nPlainLenMin,nPlainLenMax))
return false;
if (!SetRainbowTableIndex(nRainbowTableIndex))
{
printf("invalid rainbow table index %d\n",nRainbowTableIndex);
return false;
}
return true;
}
string CChainWalkContext::GetHashRoutineName()
{
return m_sHashRoutineName;
}
int CChainWalkContext::GetHashLen()
{return m_nHashLen;
}
string CChainWalkContext::GetPlainCharsetName()
{
return m_sPlainCharsetName;
}
string CChainWalkContext::GetPlainCharsetContent()
{
return m_sPlainCharsetContent;
}
int CChainWalkContext::GetPlainLenMin()
{
return m_nPlainLenMin;
}
int CChainWalkContext::GetPlainLenMax()
{
return m_nPlainLenMax;
}
uint64 CChainWalkContext::GetPlainSpaceTotal()
{
return m_nPlainSpaceTotal;
}
int CChainWalkContext::GetRainbowTableIndex()
{
return m_nRainbowTableIndex;
}
void CChainWalkContext::Dump()
{
printf("hash routine: %s\n", m_sHashRoutineName.c_str());
printf("hash length: %d\n", m_nHashLen);
printf("plain charset: ");
int i;
for (i = 0; i < m_nPlainCharsetLen; i++)
{
if (isprint(m_PlainCharset[i]))
printf("%c", m_PlainCharset[i]);
else
printf("?");
}
printf("\n");
printf("plain charset in hex: ");
for (i = 0; i < m_nPlainCharsetLen; i++)
printf("%02x ", m_PlainCharset[i]);
printf("\n");
printf("plain length range: %d - %d\n", m_nPlainLenMin,m_nPlainLenMax);
printf("plain charset name: %s\n", m_sPlainCharsetName.c_str());
//printf("plain charset content: %s\n",m_sPlainCharsetContent.c_str());
//for (i = 0; i <= m_nPlainLenMax; i++)
// printf("plain space up to %d: %s\n", i,uint64tostr(m_nPlainSpaceUpToX[i]).c_str());
printf("plain space total: %s\n",uint64tostr(m_nPlainSpaceTotal).c_str());
printf("rainbow table index: %d\n", m_nRainbowTableIndex);
printf("reduce offset: %s\n",uint64tostr(m_nReduceOffset).c_str());
printf("\n");
}
void CChainWalkContext::GenerateRandomIndex()
{
// create 8 random bytes, which is then used as a 64 bit random number
RAND_bytes((unsigned char*)&m_nIndex, 8);
// use this random number modulo the size of the total key space to get an index that is guaranteed
// to be less than the size of the m_nPlainSpaceTotal.
// NOTE: On the x86 architecture, this is stored on disk in little-endian format (little end first), so the byte
// streams will look something like this: BF9A09BC 00000000,should the m_nPlainSpaceTotal be 0x00000000FFFFFFFF
// and the integer would be: 3154746047. Windows calculator would display this as: BC09 9ABF
m_nIndex = m_nIndex % m_nPlainSpaceTotal;
}
void CChainWalkContext::SetIndex(uint64 nIndex)
{
m_nIndex = nIndex;
}
void CChainWalkContext::SetHash(unsigned char* pHash)
{
memcpy(m_Hash, pHash, m_nHashLen);
}
// convert the index into a character string. Lots of math is done hereto map the numeric index into a simple array of characters.
// this is essentially using a different base counting system
void CChainWalkContext::IndexToPlain()
{
int i;
// Find the length of the plaintext that this index points to and store it in m_nPlainLen.
// It is entirely possible that it could be anything between the min/max lengths specified as the plain length range
for (i = m_nPlainLenMax - 1; i >= m_nPlainLenMin - 1; i--)
{
if (m_nIndex >= m_nPlainSpaceUpToX[i])
{
m_nPlainLen = i + 1;
break;
}
}
uint64 nIndexOfX = m_nIndex - m_nPlainSpaceUpToX[m_nPlainLen -
1];
/*
// Slow version
for (i = m_nPlainLen - 1; i >= 0; i--)
{
m_Plain[i] = m_PlainCharset[nIndexOfX %
m_nPlainCharsetLen];
nIndexOfX /= m_nPlainCharsetLen;
}
*/
// Fast version
for (i = m_nPlainLen - 1; i >= 0; i--)
{
#ifdef _WIN32
// break to the 32 bit version of this code if nIndexOfX is a 32 bit unsigned integer
if (nIndexOfX < 0x100000000I64)
break;
#else
if (nIndexOfX < 0x100000000llu)
break;
#endif
// create a plaintext character string one character at a time, based on the index and starting with the
// least significant character.
m_Plain[i] = m_PlainCharset[nIndexOfX %m_nPlainCharsetLen];
nIndexOfX /= m_nPlainCharsetLen;
}
// this is the 32 bit version of the above code. It uses assembly to be much faster
unsigned int nIndexOfX32 = (unsigned int)nIndexOfX;
for (; i >= 0; i--)
{
//m_Plain[i] = m_PlainCharset[nIndexOfX32 %m_nPlainCharsetLen];
//nIndexOfX32 /= m_nPlainCharsetLen;
unsigned int nPlainCharsetLen = m_nPlainCharsetLen;
unsigned int nTemp;
#ifdef _WIN32
__asm
{
mov eax, nIndexOfX32
xor edx, edx
div nPlainCharsetLen
mov nIndexOfX32, eax
mov nTemp, edx
}
#else
__asm__ __volatile__ ( "mov %2, %%eax;"
"xor %%edx, %%edx;"
"divl %3;"
"mov %%eax, %0;"
"mov %%edx, %1;"
: "=m"(nIndexOfX32),
"=m"(nTemp)
: "m"(nIndexOfX32),
"m"(nPlainCharsetLen)
: "%eax", "%edx"
);
#endif
m_Plain[i] = m_PlainCharset[nTemp];
}
}
void CChainWalkContext::PlainToHash()
{
m_pHashRoutine(m_Plain, m_nPlainLen, m_Hash);
}
// I think this is basically a reduction function. The m_nReduceOffset will be unique for each
// numbered rainbow table. Even within the same rainbow table, the nPos that is used will assist
// in ensuring that the index values are unique from hashes that eventually map to the same value
// somewhere in the chain
void CChainWalkContext::HashToIndex(int nPos)
{
// treat the hash as a pointer to a uint64. Take the value and add m_nReduceOffset + nPos
// then mod this by m_nPlainSpaceTotal to get the index
// NOTE: This essentially just uses the first 64 bits of the hash and introduces a reduction function
// to help make sure that it is unique on this iteration.
// NOTE: It appears that the use of the m_nReduceOffset is to keep this reduction function
// unique even between rainbow table files
m_nIndex = (*(uint64*)m_Hash + m_nReduceOffset + nPos) % m_nPlainSpaceTotal;
}
uint64 CChainWalkContext::GetIndex()
{
return m_nIndex;
}
string CChainWalkContext::GetPlain()
{
string sRet;
int i;
for (i = 0; i < m_nPlainLen; i++)
{
char c = m_Plain[i];
if (c >= 32 && c <= 126)
sRet += c;
else
sRet += '?';
}
return sRet;
}
string CChainWalkContext::GetBinary()
{
return HexToStr(m_Plain, m_nPlainLen);
}
string CChainWalkContext::GetPlainBinary()
{
string sRet;
sRet += GetPlain();
int i;
for (i = 0; i < m_nPlainLenMax - m_nPlainLen; i++)
sRet += ' ';
sRet += "|";
sRet += GetBinary();
for (i = 0; i < m_nPlainLenMax - m_nPlainLen; i++)
sRet += " ";
return sRet;
}
string CChainWalkContext::GetHash()
{
return HexToStr(m_Hash, m_nHashLen);
}
bool CChainWalkContext::CheckHash(unsigned char* pHash)
{
if (memcmp(m_Hash, pHash, m_nHashLen) == 0)
return true;
return false;
}