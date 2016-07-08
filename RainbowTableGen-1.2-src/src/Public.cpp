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
#include "Public.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/sysinfo.h>
#endif
//////////////////////////////////////////////////////////////////////
unsigned int GetFileLen(FILE* file)
{
unsigned int pos = ftell(file);
fseek(file, 0, SEEK_END);
unsigned int len = ftell(file);
fseek(file, pos, SEEK_SET);
return len;
}
string TrimString(string s)
{
while (s.size() > 0)
{
if (s[0] == ' ' || s[0] == '\t')
s = s.substr(1);
else
break;
}
while (s.size() > 0)
{
if (s[s.size() - 1] == ' ' || s[s.size() - 1] == '\t')
s = s.substr(0, s.size() - 1);
else
break;
}
return s;
}
bool ReadLinesFromFile(string sPathName, vector<string>& vLine)
{
vLine.clear();
FILE* file;
///+++++++++++++++++++++++++++
file=fopen(sPathName.c_str(), "rb");
//fopen_s(&file,sPathName.c_str(), "rb");
if (file != NULL)
{
unsigned int len = GetFileLen(file);
char* data = new char[len + 1];
fread(data, 1, len, file);
data[len] = '\0';
string content = data;
content += "\n";
delete data;
int i;
for (i = 0; i < (int)content.size(); i++)
{
if (content[i] == '\r')
content[i] = '\n';
}
int n;
while ((n = (int)content.find("\n", 0)) != -1)
{
string line = content.substr(0, n);
line = TrimString(line);
if (line != "")
vLine.push_back(line);
content = content.substr(n + 1);
}
fclose(file);
}
else
return false;
return true;
}
bool SeperateString(string s, string sSeperator, vector<string>& vPart)
{
vPart.clear();
int i;
for (i = 0; i < (int)sSeperator.size(); i++)
{
int n = (int)s.find(sSeperator[i]);
if (n != -1)
{
vPart.push_back(s.substr(0, n));
s = s.substr(n + 1);
}
else
return false;
}
vPart.push_back(s);
return true;
}
string uint64tostr(uint64 n)
{
char str[32];
#ifdef _WIN32
///+++++++++++++++++++++++++++
//sprintf_s(str, "%I64u", n);
sprintf(str, "%I64u", n);
#else
sprintf(str, "%llu", n);
#endif
return str;
}
string uint64tohexstr(uint64 n)
{
char str[32];
#ifdef _WIN32
///++++++++++++++++++++++++++++++++++
//sprintf_s(str, "%016I64x", n);
sprintf(str, "%016I64x", n);
#else
sprintf(str, "%016llx", n);
#endif
return str;
}
string HexToStr(const unsigned char* pData, int nLen)
{
string sRet;
int i;
for (i = 0; i < nLen; i++)
{
char szByte[3];
#ifdef _WIN32
///++++++++++++++++++++++++++++++++++++++++
//sprintf_s(szByte, "%02x", pData[i]);
sprintf(szByte, "%02x", pData[i]);
#else
sprintf(szByte, "%02x", pData[i]);
#endif
sRet += szByte;
}
return sRet;
}
unsigned int GetAvailPhysMemorySize()
{
#ifdef _WIN32
MEMORYSTATUS ms;
GlobalMemoryStatus(&ms);
return (unsigned int)ms.dwAvailPhys;
#else
struct sysinfo info;
sysinfo(&info); // This function is Linux-specific
return info.freeram;
#endif
}
void ParseHash(string sHash, unsigned char* pHash, int& nHashLen)
{
int i;
for (i = 0; i < (int)sHash.size() / 2; i++)
{
string sSub = sHash.substr(i * 2, 2);
int nValue;
#ifdef _WIN32
///+++++++++++++++++++++++++++
//sscanf_s(sSub.c_str(), "%02x", &nValue);
sscanf(sSub.c_str(), "%02x", &nValue);
#else
sscanf(sSub.c_str(), "%02x", &nValue);
#endif
pHash[i] = (unsigned char)nValue;
}
nHashLen = (int)sHash.size() / 2;
}
void Logo()
{
printf("MPI RainbowCrack 1.0 - Making a Faster Cryptanalytic Time-Memory Trade-Off\n");
//printf("by Mike Taber <mstaber@gmail.com>\n");
//printf("http://www.miketaber.net/\n\n");
//printf("Reference Code based on:\n");
//printf("RainbowCrack 1.2 - Making a Faster Cryptanalytic Time- Memory Trade-Off\n");
printf("by xuefei <gaofeifeitian@gmail.com>\n");
//printf("http://www.antsight.com/zsl/rainbowcrack/\n\n");
}
void mySleep( int milliseconds )
{
#ifdef _WIN32
Sleep(milliseconds);
#else
sleep(milliseconds);
#endif
}
// take a numeric value as a string and make it a comma delimited number
string CommaDelimitedNumber(long l)
{
char n[1024];
sprintf(n,"%d",l);
string numbers = n;
string newNumbers;
bool containsDecimal = false;
// see if the number contains a decimal value
// technically, this shouldn't happen with a long value
string::size_type loc = numbers.find( ".", 0 );
string postDecimal = "";
if( loc != string::npos )
{
postDecimal = numbers.substr(loc);
numbers = numbers.substr(0,loc);
}
// take into account situations where there are a multiple of 3 numbers. If we don't do this,
// we might very well run into a problem with a preceding comma
if( numbers.length()%3 == 0 )
{
newNumbers = numbers.substr(0,3);
numbers = numbers.substr(3);
}
else
{
newNumbers = numbers.substr(0,numbers.length()%3);
numbers = numbers.substr(numbers.length()%3);
}
// now continue to truncate the string in groups of 3 characters until it is gone.
while( numbers.length() > 0 )
{
newNumbers += "," + numbers.substr(0,3);
numbers = numbers.substr(3);
}
return newNumbers + postDecimal;
}