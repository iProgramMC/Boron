#include <main.h>
#include <ke.h>
#include <ex.h>
#include <string.h>
#include <hal.h>

KMUTEX GlobalMutex;

void CreateThread(PKTHREAD_START StartRoutine, int Parameter)
{
	PKTHREAD Thread = KeCreateEmptyThread();
	
	KeInitializeThread(
		Thread,
		EX_NO_MEMORY_HANDLE,
		StartRoutine,
		(void*)(uintptr_t) Parameter);
	
	LogMsg("Creating thread %d, it's %p", Parameter, Thread);
	
	KeSetPriorityThread(Thread, PRIORITY_NORMAL);
	
	KeReadyThread(Thread);
}

NO_RETURN void TestThread(UNUSED void* Parameter)
{
	int ThreadNum = (int)(uintptr_t)Parameter;
	
	DbgPrint("Hello from TestThread");
	
	while (true)
	{
		LogMsg("Thread %d Acquiring Mutex...", ThreadNum);
		// acquire the mutex
		KeWaitForSingleObject(&GlobalMutex, false, TIMEOUT_INFINITE);
		
		LogMsg("Thread %d Waiting A Second...", ThreadNum);
		// wait a second
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, 1000, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
		
		// then release the mutex
		LogMsg("Thread %d Releasing Mutex...", ThreadNum);
		KeReleaseMutex(&GlobalMutex);
	}
}

int DriverEntry()
{
	KeInitializeMutex(&GlobalMutex, 1);
	
	CreateThread(TestThread, 1);
	CreateThread(TestThread, 2);
	
	
	return 0;
}
