#include "utils.h"
#include "tests.h"

PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter)
{
	PKTHREAD Thread = KeAllocateThread();
	
	KeInitializeThread(
		Thread,
		EX_NO_MEMORY_HANDLE,
		StartRoutine,
		Parameter,
		KeGetSystemProcess());
	
	KeSetPriorityThread(Thread, PRIORITY_NORMAL);
	
	KeReadyThread(Thread);
	return Thread;
}

NO_RETURN void DriverTestThread(UNUSED void* Parameter)
{
	PerformProcessTest();
	//PerformMutexTest();
	//PerformBallTest();
	
	LogMsg(ANSI_GREEN "*** All tests have concluded." ANSI_RESET);
	KeTerminateThread();
}

int DriverEntry()
{
	KeDetachThread(CreateThread(DriverTestThread, NULL));
	
	return 0;
}
