/*
PRTGen - A Parallel implementation of RainbowCrack using MPI.
Copyright (C) 2008 Mike Taber <mstaber@gmail.com>
This source code is based on:
RainbowCrack - a general propose implementation of Philippe
Oechslin's faster time-memory trade-off technique.
Copyright (C) Zhu Shuanglei <shuanglei@hotmail.com>
*/
/*
MT: To statically link the libraries in Visual Studio,
go to: Project + Properties, C/C++, Code Generation, Runtime
library. Change the setting to "Multi-threaded (/MT)"
*/
#include "mpi.h"
#ifdef _WIN32
#pragma warning(disable : 4786)
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <time.h>
#include <winsock.h>
#include "ChainWalkContext.h"
#include "Benchmark.h"
// define all of the different types of messages that will be sent
#define TAG_IO_STRING 0 // Defines text sent from MPI processes to root process that will be printed to the screen
#define TAG_BENCHMARK_SPEED 1 // This identifies an incoming benchmark item
#define TAG_SYNC 2 // Synchronize all of the processes
#define TAG_WORK_UNIT 3 // Used to send work units to the target processes
#define TAG_REQUEST_FINISHED_WORK_UNITS 4 // Used to request finished work units from the child processes
#define TAG_FINISHED_WORK_UNIT 5 // Used to send finished work units to the main process
#define TAG_SLAVE_WORKING_TIME 6 // Used to send the working time back to the main process
// define the root process
#define DEST_ROOT 0
#define SOURCE_ROOT 0
#define WORK_PACKET_SIZE 2*1024
// Print out usage information
// The reason this function returns a value is so that it can be called with a conditional statement to check the process rank
int Usage()
{
Logo();
printf("usage: rtgen hash_algorithm charset minlen maxlen table_index chain_len chain_count file_suffix timeslice\n");
printf(" rtgen hash_algorithm charset minlen maxlen table_index chain_len chain_count file_suffix timeslice -bench\n");
printf("\n");
CHashRoutine hr;
printf("hash_algorithm: available: %s\n", hr.GetAllHashRoutineName().c_str());
printf("charset: use any charset name in charset.txt here\n");
printf(" use \"byte\" to specify all 256 characters as the charset of the plaintext\n");
printf("minlen: min length of the plaintext\n");
printf("maxlen: max length of the plaintext\n");
printf("table_index: index of the rainbow table\n");
printf("chain_len: length of the rainbow chain\n");
printf("chain_count: count of the rainbow chain to generate\n");
printf("file_suffix: the string appended to the file title\n");
printf(" add your comment of the generated rainbow table here\n");
printf("timeslice approximate time of each work unit, in seconds\n");
printf("-bench: do benchmarking, but no processing\n");
printf("\n");
printf("example: rtgen lm alpha 1 7 0 100 16 test\n");
printf(" rtgen md5 byte 4 4 0 100 16 test\n");
printf(" rtgen sha1 numeric 1 10 0 100 16 test\n");
printf(" rtgen sha1 numeric 1 10 0 100 16 test - bench\n");
return 0;
}
int main2(int argc, char* argv[])
{
char myhostname[256];
bool bDoBenchmark = false;
list<Benchmark> L;
Benchmark benchmarkItem;
/////////////////////////////////////
// Get the hostname of the computer
// NOTE: This is likely platform specific to Windows
/////////////////////////////////////
WSADATA wsa_data;
/* Load Winsock 2.0 DLL */
if (WSAStartup(MAKEWORD(2, 0), &wsa_data) != 0)
{
printf("WSAStartup() failed\n");
return (1);
}
int rc = gethostname(myhostname, sizeof(myhostname));
WSACleanup(); /* Cleanup Winsock */
///////////////////////////////////
//////////////////
// Set up MPI
//////////////////
int my_rank;
int p; // number of processes
int source; // rank of sender
int dest; // rank of destination
int tag = 0;
MPI_Status status;
// start MPI
MPI_Init(&argc, &argv);
// find out my process rank
MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
// get number of processes
MPI_Comm_size(MPI_COMM_WORLD, &p);
// validate the number of arguments that were passed
if (argc == 11)
{
if (strcmp(argv[10], "-bench") == 0)
{
bDoBenchmark = true;
}
else
{
// invalid number of arguments. Print this out on the main process
my_rank == 0 && Usage();
return 0;
}
}
string exePath;
string sHashRoutineName;
string sCharsetName;
int nPlainLenMin;
int nPlainLenMax = 0;
int nRainbowTableIndex = 0;
long nRainbowChainLen = 0;
long nRainbowChainCount = 0;
string sFileSuffix;
int timeslice = 0;
// Before we start any "real work", do some benchmarking on each of the target computers so that we
// have some idea of how long this entire job is going to take
if (argc == 10 || bDoBenchmark )
{
// assign the command line parameters to variables so it's easy to know what we're working with
exePath = argv[0];
// NOTE: exePath is calculated and does not include the last "\" after this line
exePath = exePath.substr(0,exePath.find_last_of("/\\"));
sHashRoutineName = argv[1];
sCharsetName = argv[2];
nPlainLenMin = atoi(argv[3]);
nPlainLenMax = atoi(argv[4]);
nRainbowTableIndex = atoi(argv[5]);
nRainbowChainLen = atol(argv[6]);
nRainbowChainCount = atol(argv[7]);
sFileSuffix = argv[8];
timeslice = atoi(argv[9]);
// nRainbowChainCount check
if (nRainbowChainCount >= 134217728 && my_rank == 0)
{
printf("This will generate a table larger than 2GB, which is not supported\n");
printf("Please use a smaller rainbow_chain_count(less than 134217728)\n");
return -1;
}
else if (nRainbowChainCount >= 134217728)
{
return -1;
}
// Setup CChainWalkContext
if (!CChainWalkContext::SetHashRoutine(sHashRoutineName) && my_rank == 0)
{
printf("Hash routine %s not supported\n", sHashRoutineName.c_str()); 
fflush(stdout);
return 0;
}
else if (nRainbowChainCount >= 134217728 && my_rank != 0)
{
return -1;
}
// load the plaintext, then calculate the key space and some other data concerning the keyspace
if(!CChainWalkContext::SetPlainCharset(exePath,sCharsetName,nPlainLenMin, nPlainLenMax) && my_rank == 0)
{
return -1;
}
else if (nRainbowChainCount >= 134217728 && my_rank != 0)
{
return -1;
}
// The RainbowTableIndex is used to determine which file is being generated.
// There is a m_nReduceOffset that is set, whose exact purpose is unknown right now
if(!CChainWalkContext::SetRainbowTableIndex(nRainbowTableIndex) && my_rank == 0)
{
printf("Invalid rainbow table index %d\n", nRainbowTableIndex);
return -1;
}
else if (nRainbowChainCount >= 134217728 && my_rank != 0)
{
return -1;
}
// if we are only using one process, kill the application, as nothing will get done
if( p <= 1 )
{
printf("ERROR: You must execute this application with more than one process.");
return -1;
}
// Do some minimal error checking before trying to run the benchmark
// Setup CChainWalkContext
if (!CChainWalkContext::SetHashRoutine(sHashRoutineName))
{
my_rank == 0 && printf("hash routine %s not supported\n", sHashRoutineName.c_str());
return -1;
}
if (!CChainWalkContext::SetPlainCharset(exePath, sCharsetName, nPlainLenMin, nPlainLenMax))
return -1;
if (!CChainWalkContext::SetRainbowTableIndex(nRainbowTableIndex))
{
my_rank == 0 && printf("invalid rainbow table index %d\n", nRainbowTableIndex);
return -1;
}
// make sure the timeslice isn't negative
if( timeslice <= 0 )
{
my_rank == 0 && printf("timeslice must be a positive integer");
return -1;
}
else if ( my_rank == 0 )
{
printf("\nTimeslice:\n");
printf("%d seconds\n", timeslice);
printf("%d minutes\n\n", timeslice/60);
fflush(stdout);
}
// Only the main process will print out any information
if( my_rank == 0 )
{
// print all of the information that has been set in the different classes and display it to the user
CChainWalkContext::Dump();
}
// set the number of hashes that will be used to find a benchmark.
int benchmarkLength = 5000000; // 5 million seems like a reasonable amount to benchmark
// Perform the actual benchmarking, but only for the nonroot processes
if( my_rank == 0)
{
// the main process will collect info, while the other processes will calculate and return values
printf("Processing benchmark speeds for %s hashing:\n", sHashRoutineName.c_str());
fflush(stdout);
long speed = 0;
long totalSpeed = 0;
long chainSpeed = 0;
char strComputer[256];
memset(strComputer,0,256);
// retrieve the benchmarking information from each of the processes
for(source = 1; source < p; source++)
{
MPI_Recv(&speed, 1, MPI_LONG, source,
TAG_BENCHMARK_SPEED, MPI_COMM_WORLD, &status);
MPI_Recv(&strComputer, sizeof(strComputer),
MPI_CHAR, source, TAG_IO_STRING, MPI_COMM_WORLD, &status);
// set up the item and add a copy to the list
benchmarkItem.hostname = strComputer;
benchmarkItem.processID = source;
benchmarkItem.speed = speed;
L.push_back(benchmarkItem);
printf("Process %d on %s: %s hashes/s, %s chains/s\n", source, strComputer, CommaDelimitedNumber(speed).c_str(),CommaDelimitedNumber(speed/nRainbow ChainLen).c_str());
totalSpeed += speed;
chainSpeed += speed/nRainbowChainLen;
}
printf("Total Speed: %s hashes per second\n",CommaDelimitedNumber(totalSpeed).c_str());
printf("Total Speed: %s chains per second\n",CommaDelimitedNumber(chainSpeed).c_str());
printf("The optimal time on these processors is approximately:\n");
double totalTime = ((double)nRainbowChainLen*(double)nRainbowChainCount/(double)totalSpeed );
printf("%16.4f seconds\n",totalTime);
printf("%16.4f minutes\n",totalTime/60);
printf("%16.4f hours\n",totalTime/3660);
printf("%16.4f days\n",totalTime/86400);
printf("%16.4f years\n",totalTime/(365*86400));
printf("\nCalculating reference speed...\n");
fflush(stdout);
mySleep(1000); // wait for 1 second for the I/O buffer to fully clear out
// Anyone using this code may ignore the following comment as irrelevant to this application.
// There¡¯s no Kelp in your violence
// Now test the root process and use it to benchmark a "reference speed". The "reference speed"
// is the speed at which a single process on the computer where this job is started would complete the entire task
// on its own.
int referenceSpeed = 0;
// Benchmark the reference node
{
CChainWalkContext cwc;
cwc.GenerateRandomIndex();
clock_t t1 = clock();
int nLoop = benchmarkLength;
int i;
for (i = 0; i < nLoop; i++)
{
cwc.IndexToPlain();
cwc.PlainToHash();
cwc.HashToIndex(i);
}
clock_t t2 = clock();
float fTime = 1.0f * (t2 - t1) / CLOCKS_PER_SEC;
referenceSpeed = long(nLoop / fTime);
}
printf("Reference Speed: %s hashes per second\n",CommaDelimitedNumber(referenceSpeed).c_str());
printf("Reference Speed: %s chains per second\n",CommaDelimitedNumber(referenceSpeed/nRainbowChainLen).c_str()
);
double referenceTime = ((double)nRainbowChainLen*(double)nRainbowChainCount/(double)referenceS peed);
printf("Using only the reference process on the computer named \"%s\", this task would take approximately:\n", myhostname);
printf("%16.4f seconds\n",referenceTime);
printf("%16.4f minutes\n",referenceTime/60);
printf("%16.4f hours\n",referenceTime/3660);
printf("%16.4f days\n",referenceTime/86400);
printf("%16.4f years\n",referenceTime/(365*86400));
fflush(stdout);
// Notify all of the other threads that they can start going now
int start = 1;
MPI_Bcast(&start, 1, MPI_INT, SOURCE_ROOT,
MPI_COMM_WORLD);
}
else
{
// Benchmark step
{
CChainWalkContext cwc;
cwc.GenerateRandomIndex();
clock_t t1 = clock();
int nLoop = benchmarkLength;
int i;
long speed=0;
for (i = 0; i < nLoop; i++)
{
cwc.IndexToPlain();
cwc.PlainToHash();
cwc.HashToIndex(i);
}
clock_t t2 = clock();
float fTime = 1.0f * (t2 - t1) / CLOCKS_PER_SEC;
speed = long(nLoop / fTime);
MPI_Send(&speed, 1, MPI_LONG, DEST_ROOT,
TAG_BENCHMARK_SPEED, MPI_COMM_WORLD);
MPI_Send(&myhostname, sizeof(myhostname),
MPI_CHAR, DEST_ROOT, TAG_IO_STRING, MPI_COMM_WORLD);
}
// Do a busy wait while the main process completes its reference benchmarking
// We don't need to check what the value received was.
// We just care that it was received, as it was a synchronization message.
int start = 1;
MPI_Bcast(&start, 1, MPI_INT, SOURCE_ROOT,
MPI_COMM_WORLD);
}
}
else
{
// an invalid number of arguments were passed.
// the main process should print out usage info, while the rest should end
my_rank == 0 && Usage();
return 0;
}
long chainsPerWorkSlice = 0;
long totalNumWorkSlices = 0;
long totalRainbowChainCount = nRainbowChainCount;
// print the contents of the benchmark List
if ( my_rank == 0 )
{
list<Benchmark>::iterator i;
L.sort(); // sort the list of nodes by speed
// workslices are measured in whole chains
chainsPerWorkSlice = (L.front().speed * timeslice) / (nRainbowChainLen);
printf("\nChains per work slice: %d. Each should take approximately %d seconds on the slowest CPU\n",chainsPerWorkSlice,timeslice);
totalNumWorkSlices = nRainbowChainCount/chainsPerWorkSlice + 1;
printf("Number of workslices=%d\n",(long)totalNumWorkSlices);
// do error correction if there's only going to be 1 work slice
if( chainsPerWorkSlice >= nRainbowChainCount )
{
chainsPerWorkSlice = nRainbowChainCount;
}
printf("Number of chains=%d, Workslices*chainsPerWorkSlice=%d\n", nRainbowChainCount, totalNumWorkSlices*chainsPerWorkSlice);
fflush(stdout);
}
// FUTURE: Calculate the expected success rate here
// if we were only benchmarking the task, we should stop here
if ( bDoBenchmark )
{return 0;
}
/*
Thread priority should be handled by MPI, not by this application
// Low priority
#ifdef _WIN32
SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
#else
nice(19);
#endif
*/
// Thanks for everything Odie!
// set up the output filename
char szFileName[256];
char szMPIFileName[256];
char szlogFileName[256];
long data[2];
data[0] = data[1] = 0;
#ifdef _WIN32
sprintf_s(szFileName, "%s\\%s_%s#%d-%d_%d_%dx%d_%s.rt", 
exePath.c_str(),
sHashRoutineName.c_str(),
sCharsetName.c_str(),
nPlainLenMin,
nPlainLenMax,
nRainbowTableIndex,
nRainbowChainLen,
nRainbowChainCount,
sFileSuffix.c_str());
sprintf_s(szMPIFileName, "%s\\%s_%s#%d-%d_%d_%dx%d_%s.MPI%02d.rt",
exePath.c_str(),
sHashRoutineName.c_str(),
sCharsetName.c_str(),
nPlainLenMin,
nPlainLenMax,
nRainbowTableIndex,
nRainbowChainLen,
nRainbowChainCount,
sFileSuffix.c_str(),
my_rank);
sprintf_s(szlogFileName, "%s\\%s_%s#%d-%d_%d_%dx%d_%s.logfile.txt",
exePath.c_str(),
sHashRoutineName.c_str(),
sCharsetName.c_str(),
nPlainLenMin,
nPlainLenMax,
nRainbowTableIndex,
nRainbowChainLen,
nRainbowChainCount,
sFileSuffix.c_str());
#else
sprintf(szFileName, "%s\\%s_%s#%d-%d_%d_%dx%d_%s.rt", exePath.c_str(),
sHashRoutineName.c_str(),
sCharsetName.c_str(),
nPlainLenMin,
nPlainLenMax,
nRainbowTableIndex,
nRainbowChainLen,
nRainbowChainCount,
sFileTitleSuffix.c_str());
#endif
char myMessage[1024];
memset(myMessage,0,1024);
long worksliceID = 0;
// Open file in append mode and immediately close it. This will create the file if it doesn't exist
if( my_rank == 0 )
{
// the first process will handle all incoming status messages from the other processes and print them to the screen
// if a "Done" message is received, then nothing is printed, but a counter is incremented indicating that
// the process is finished doing work
// NOTE: An assumption is made here that incoming messages do not end with a newline, so this process takes care of that
memset(myMessage,0,1024);
// first, send a message to each process instructing it how many work units it needs to do
// long chainsPerWorkSlice = 0;
// long totalNumWorkSlices = 0;
int i = 1;
worksliceID = 1;
printf("\nStarting work...\n\n");
fflush(stdout);
// t1 is used to calculate the total time to completion from start to finish in the root node
clock_t timeSlavesStarted = clock();
while( i < p )
{
// send one workslice to each process
if( chainsPerWorkSlice >= nRainbowChainCount )
{
chainsPerWorkSlice = nRainbowChainCount;
} // do any corrections as needed
data[0] = worksliceID;
data[1] = chainsPerWorkSlice;
MPI_Send(&data, 2, MPI_LONG, i, TAG_WORK_UNIT,
MPI_COMM_WORLD);
nRainbowChainCount -= chainsPerWorkSlice;
worksliceID++;
i++;
}
// create a logfile that status messages will be sent to
FILE* logfile;
// create the file if it doesn't exist
fopen_s(&logfile, szlogFileName, "w"); //overwrite the file if it exists
fclose(logfile);
fopen_s(&logfile, szlogFileName, "r+"); // open in read/write binary mode
if (logfile == NULL)
{
printf("failed to create %s on the root node\n",
szlogFileName);
return 0;
}
// start handing out work units to all of the processes
while( nRainbowChainCount > 0 )
{// now listen for responses saying that a workslice is finished and send a new unit back to that process
MPI_Status status;
// do a busy-wait for all incoming messages
memset(myMessage,0,1024);
MPI_Recv(&myMessage, 1024, MPI_CHAR, MPI_ANY_SOURCE,
MPI_ANY_TAG, MPI_COMM_WORLD, &status);
// log the incoming message to a logfile
fwrite(myMessage, 1, strlen(myMessage), logfile);
fwrite("\n", 1, 1, logfile);
// assign a new work unit that contains the "ID" of this workslice, and the number of chains that need to be generated
if( chainsPerWorkSlice >= nRainbowChainCount )
{
chainsPerWorkSlice = nRainbowChainCount;
} // do any corrections as needed
data[0] = worksliceID;
data[1] = chainsPerWorkSlice;
MPI_Send(&data, 2, MPI_LONG, status.MPI_SOURCE,
TAG_WORK_UNIT, MPI_COMM_WORLD);
nRainbowChainCount -= chainsPerWorkSlice;
worksliceID++;
}
// tell each work unit that checks in from now on that they're done
i = 1;
while(i < p)
{
data[0] = 0;
data[1] = 0;
memset(myMessage,0,1024);
MPI_Recv(&myMessage, 1024, MPI_CHAR, MPI_ANY_SOURCE,
MPI_ANY_TAG, MPI_COMM_WORLD, &status);
// log the incoming message to a logfile
fwrite(myMessage, 1, strlen(myMessage), logfile);
fwrite("\n", 1, 1, logfile);
// reply to the latest process and tell him that he's done
MPI_Send(&data, 2, MPI_LONG, status.MPI_SOURCE,
TAG_WORK_UNIT, MPI_COMM_WORLD); // assign a new work unit of size zero
i++;
// Track the time that the process was completed
list<Benchmark>::iterator i;
for(i=L.begin(); i != L.end(); ++i)
{
if ( i->processID == status.MPI_SOURCE )
{
i->waitingTimeStart = clock();
}
}
}
clock_t timeSlavesFinished = clock();
fclose(logfile); // close the logfile
double dTotalTime = (double)(timeSlavesFinished -
timeSlavesStarted + 0.0) / CLOCKS_PER_SEC;
printf("\nFinished %s chains in:\n", CommaDelimitedNumber(totalRainbowChainCount).c_str());
printf("%16.4f seconds\n",dTotalTime);
printf("%16.4f minutes\n",dTotalTime/60);
printf("%16.4f hours\n",dTotalTime/3660);
printf("%16.4f days\n",dTotalTime/86400);
printf("%16.4f years\n\n",dTotalTime/(365*86400));
fflush(stdout);
/////////////////////////////////////////////////////////////////
/////////
// Collect the working times for each node
/////////////////////////////////////////////////////////////////
/////////
MPI_Bcast(myMessage,1,MPI_CHAR,0,MPI_COMM_WORLD); // tell the slave processes to start sending their working times
float workingTime = 0.0f;
for( dest = 1; dest < p; dest++)
{
workingTime = 0.0f;
MPI_Recv(&workingTime, 1, MPI_FLOAT, dest,
TAG_SLAVE_WORKING_TIME, MPI_COMM_WORLD, &status);
list<Benchmark>::iterator iter;
for(iter=L.begin(); iter != L.end(); ++iter)
{
if( iter->processID == status.MPI_SOURCE )
{
iter->dWorkingTime = workingTime;
// printf("Process %d on %s worked for %16.4f
seconds\n",iter->processID, iter->hostname.c_str(), iter->workingTime);
}
}
}
/////////////////////////////////////////////////////////////////
/////////
// Perform various calculations for the working, waiting,
and idle times.
// Then display that information in a nice table
/////////////////////////////////////////////////////////////////
/////////
list<Benchmark>::iterator iter;
double dTotalWorkingTime = 0.0f;
double dTotalIdleTime = 0.0f;
double dTotalWaitingTime = 0.0f;
printf("[Process ID]:[hostname]\t[Working Time] \t [Idle Time] \t [Waiting Time]\n");
for( int j = 0; j <= (int)L.size(); j++ )
{
for(iter=L.begin(); iter != L.end(); ++iter)
{
if( iter->processID == j )
{
iter->dWaitingTime = (double)(timeSlavesFinished - iter->waitingTimeStart + 0.0) / CLOCKS_PER_SEC;
iter->dIdleTime = dTotalTime - iter->dWorkingTime - iter->dWaitingTime; // idle time is calculated
printf("%d:%s\t%16.4f %16.4f %16.4f\n",iter->processID, iter->hostname.c_str(), iter->dWorkingTime, iter->dIdleTime, iter->dWaitingTime);
dTotalWorkingTime += iter->dWorkingTime;
dTotalIdleTime += iter->dIdleTime;
dTotalWaitingTime += iter->dWaitingTime;
}
}
}
printf("%d:%s\t\t%16.4f %16.4f %16.4f\n",0,"",dTotalWorkingTime,dTotalIdleTime,dTotalWaitingTime);
/////////////////////////////////////
// Gather the output data
/////////////////////////////////////
printf("\nGathering output data...");
fflush(stdout);
// broadcast a message to the other processes telling them to start sending data back to the master
clock_t timeGatheringStarted = clock();
memset(myMessage,0,1024);
MPI_Bcast(myMessage,1,MPI_CHAR,0,MPI_COMM_WORLD);
// do a quick error check to ensure that the nRainbowChainCount has hit zero
if( nRainbowChainCount != 0 )
{
printf("nRainbowChainCount != 0: %d\n", nRainbowChainCount);
fflush(stdout);
return 1;
}
// we simply need to start accepting data until we've gathered the number of chains that we expected to 
double chains[WORK_PACKET_SIZE]; // buffer for up to 1024 chains
FILE* file;
fopen_s(&file, szFileName, "w"); // create the file or overwrite the file if it exists
fclose(file);
fopen_s(&file, szFileName, "r+b"); // open in read/write binary mode
if (file == NULL)
{
printf("failed to create %s on the root node\n", szFileName);
return 0;
}
// receive the chains and write them to disk until we have received them all
// this assumes that all chains have been successfully written to disk.
// If they have not, then this might end up waiting forever
while( nRainbowChainCount < totalRainbowChainCount )
{
MPI_Recv(&chains, WORK_PACKET_SIZE, MPI_DOUBLE,
MPI_ANY_SOURCE, TAG_FINISHED_WORK_UNIT, MPI_COMM_WORLD, &status);
int receivedChainCount = 0;
MPI_Get_count(&status,
MPI_DOUBLE,&receivedChainCount);
receivedChainCount = receivedChainCount/2; // the first double is the starting point, and the second is the ending point
int elementsWritten = (int)fwrite(chains,
sizeof(double), receivedChainCount*2, file);
if ( elementsWritten != receivedChainCount*2 )
{
printf("disk write fail: %d elements written\n", elementsWritten);
printf("expected: %d elements written\n", receivedChainCount*2);
break;
}
nRainbowChainCount += receivedChainCount; // keep track of all of the chains being received
}
fclose(file);
clock_t timeGatheringFinished = clock();
float dGatheringTime = (double)(timeGatheringFinished - timeGatheringStarted + 0.0) / CLOCKS_PER_SEC;
printf("DONE!\n\nFinished gathering %s chains in:\n",
CommaDelimitedNumber(totalRainbowChainCount).c_str());
printf("%16.4f seconds\n",dGatheringTime);
printf("%16.4f minutes\n",dGatheringTime/60);
printf("%16.4f hours\n",dGatheringTime/3660);
printf("%16.4f days\n",dGatheringTime/86400);
printf("%16.4f years\n",dGatheringTime/(365*86400));
fflush(stdout);
}
else
{
// SLAVE NODE:
// FUTURE: Here, we could check for the existence of a file, and if it exists, we
// send it back to the master node, and tell it to recalculate the number of tasks
// create the intermediate MPI file on the target
FILE* file_tmp;
fopen_s(&file_tmp, szMPIFileName, "w"); // overwrite the file if it exists
fclose(file_tmp);
FILE* file;
fopen_s(&file, szMPIFileName, "r+b"); // open in read/write binary mode
if (file == NULL)
{
// if creating the file fails for any reason, the application will crash because MPI doesn't know what to do
printf("failed to create %s\n", szFileName);
return 0;
}
// Check existing chains
unsigned int nDataLen = GetFileLen(file);
nDataLen = nDataLen / 16 * 16; // I don't think this code does anything at all
/*
if (nDataLen == nRainbowChainCount * 16)
{
printf("precomputation of this rainbow table already finished\n");
fclose(file);
return 0;
}
if (nDataLen > 0)
{
printf("continuing from interrupted precomputation...\n");
}
*/
// TEMP CODE: THIS WILL ALWAYS START CREATING A RAINBOW TABLE FROM SCRATCH
// FUTURE: See above note about calculating the amount of work left and commented code
nDataLen = 0;
// if we're continuing an interrupted computation, go to the end. Otherwise, we're starting at the beginning, which is also the end
fseek(file, nDataLen, SEEK_SET);
// get the first assigned work unit
data[0] = data[1] = 0;
MPI_Recv(&data, 2, MPI_LONG, SOURCE_ROOT, TAG_WORK_UNIT,
MPI_COMM_WORLD, &status);
worksliceID = data[0];
chainsPerWorkSlice = data[1];
double dWorkingTime = 0.0f;
// it's possible that the work unit is empty because the time slice was too high. if so, abort execution
if ( chainsPerWorkSlice == 0 )
{
// there's no work to be done, so inform the user, receive the last packet that instructs it to do zero work, then end
memset(myMessage,0,1024);
sprintf_s(myMessage, "Process %d is not able to
process any chains due to the timeslice specified", my_rank);
MPI_Send(myMessage, (int)strlen(myMessage), MPI_CHAR,
DEST_ROOT, TAG_IO_STRING, MPI_COMM_WORLD);
// get a new work unit, which is expected to be zero in length. After this, the application ends
MPI_Recv(&data, 2, MPI_LONG, SOURCE_ROOT,
TAG_WORK_UNIT, MPI_COMM_WORLD, &status);
// wait till we hear a synchronization message from the root process before sending our "working time" back
MPI_Bcast(myMessage,1,MPI_CHAR,0,MPI_COMM_WORLD);
MPI_Send(&dWorkingTime, 1, MPI_DOUBLE, DEST_ROOT,
TAG_SLAVE_WORKING_TIME, MPI_COMM_WORLD);
// exit the application because we have no data to return to the master
return 0;
}
// while we still have work to do, continue doing it
while( chainsPerWorkSlice > 0 )
{
// Generate rainbow table
CChainWalkContext cwc;
// Starting the timer
clock_t t1 = clock();
int i;
// take into account whether or not we're picking up where we might have previously left off for any reason (crash or otherwise)
for (i = nDataLen / 16; i < chainsPerWorkSlice; i++)
{
// generate a 64-bit random index number and write it to disk.
// this number is guaranteed to be less than
the size of the plain space total
cwc.GenerateRandomIndex();
uint64 nIndex = cwc.GetIndex();
if (fwrite(&nIndex, 1, 8, file) != 8)
{// if we couldn't write 8 bytes to disk, then there was a disk write failure
printf("disk write fail\n");
break;
}
// starting with the randomly selected index:
// 1) convert it to plaintext
// 2) generate a hash
// 3) map the hash back to an index that is based on the number of times this has been hashed
int nPos;
for (nPos = 0; nPos < nRainbowChainLen - 1;
nPos++)
{
cwc.IndexToPlain(); // convert
the index into the corresponding plaintext
cwc.PlainToHash(); // create a
hash of the plaintext
cwc.HashToIndex(nPos); // convert the hash back to an Index, based in part on
//the position in the rainbow chain that we're working with
}
nIndex = cwc.GetIndex(); // this is the final index into the set of plaintext characters
if (fwrite(&nIndex, 1, 8, file) != 8)
{
printf("disk write fail\n");
break;
}
} // for (i = nDataLen / 16; i < chainsPerWorkSlice;i++)
// now that we're done with this work unit, notify the master process and listen for a new work unit
clock_t t2 = clock();
dWorkingTime += (double)(t2 - t1 + 0.0)/CLOCKS_PER_SEC; // keep track of the total working time for this slave node
int nSecond = (t2 - t1) / CLOCKS_PER_SEC;
memset(myMessage,0,1024);
sprintf_s(myMessage, "Process %d finished %d chains in workslice ID %d in (%d m %d s)", my_rank, chainsPerWorkSlice, worksliceID, nSecond / 60, nSecond % 60);
MPI_Send(myMessage, (int)strlen(myMessage), MPI_CHAR,
DEST_ROOT, TAG_IO_STRING, MPI_COMM_WORLD);
// get a new work unit
MPI_Recv(&data, 2, MPI_LONG, SOURCE_ROOT,TAG_WORK_UNIT, MPI_COMM_WORLD, &status);
worksliceID = data[0];
chainsPerWorkSlice = data[1];
}
// wait till we hear a synchronization message from the root process before sending our working time back
MPI_Bcast(myMessage,1,MPI_CHAR,0,MPI_COMM_WORLD);
MPI_Send(&dWorkingTime, 1, MPI_DOUBLE, DEST_ROOT,
TAG_SLAVE_WORKING_TIME, MPI_COMM_WORLD);
// Now that all of the chain generation has been completed, return all of the data back to the master node
nDataLen = GetFileLen(file);
fseek(file, 0, SEEK_SET);
double chains[WORK_PACKET_SIZE]; // buffer for up to 1024 chains
unsigned int totalChainsToSend = nDataLen/(2*sizeof(double));
unsigned int chainsToSend = 0;
unsigned int chainsToSendCounter = 0;
// wait till we hear from the root process before sending chains back to the root process
MPI_Bcast(myMessage,1,MPI_CHAR,0,MPI_COMM_WORLD);
// send data until there is none left to send
while( chainsToSendCounter < totalChainsToSend )
{
if( chainsToSendCounter + WORK_PACKET_SIZE/(2*sizeof(double)) > totalChainsToSend )
{
chainsToSend = totalChainsToSend - chainsToSendCounter;
}
else
{
chainsToSend = WORK_PACKET_SIZE/(2*sizeof(double));
}
memset(chains,0,WORK_PACKET_SIZE*sizeof(double));
// read the data from the file and begin to send it
unsigned int dataRead = (unsigned int)fread(chains,sizeof(double),2*chainsToSend,file);
if ( dataRead != 2*chainsToSend )
{
printf("Unable to read %d chains from file on Process %d\n",2*chainsToSend,my_rank);
fflush(stdout);
return -1;
}
MPI_Send(chains, chainsToSend*2, MPI_DOUBLE, DEST_ROOT, TAG_FINISHED_WORK_UNIT, MPI_COMM_WORLD);
chainsToSendCounter += chainsToSend;
}
// Close the output file
fclose(file);
}
return 0;
}
int main(int argc, char* argv[])
{
// call the main part of the application
main2(argc, argv);
////////////////////////////////
// Finalize the MPI Interface
// This is done here to prevent issues with the "return 0" calls that are frequently used in the main2 function
// which are in place for error handling
////////////////////////////////
MPI_Finalize();
}