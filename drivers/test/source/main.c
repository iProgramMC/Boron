#include "utils.h"
#include "tests.h"

PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter)
{
	PKTHREAD Thread = KeCreateEmptyThread();
	
	KeInitializeThread(
		Thread,
		EX_NO_MEMORY_HANDLE,
		StartRoutine,
		Parameter);
	
	LogMsg("Creating thread %d, it's %p", Parameter, Thread);
	
	KeSetPriorityThread(Thread, PRIORITY_NORMAL);
	
	KeReadyThread(Thread);
	return Thread;
}

NO_RETURN void DriverTestThread(UNUSED void* Parameter)
{
	PerformMutexTest();
	
	LogMsg(ANSI_GREEN "*** All tests have concluded." ANSI_RESET);
	KeTerminateThread();
}

int DriverEntry()
{
	CreateThread(DriverTestThread, NULL);
	
	return 0;
}
