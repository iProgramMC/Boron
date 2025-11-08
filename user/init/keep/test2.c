#include <boron.h>
#include <string.h>
#include "misc.h"

NO_RETURN
void ThreadTerminatee(UNUSED void* Context)
{
	for (int i = 0; i < 100; i++)
	{
		DbgPrint("Doing some work... %d/100", i);
		OSSleep(100);
	}
	
	DbgPrint("Huh, I'm fine.");
	OSExitThread();
}

NO_RETURN
void ThreadTerminator(void* Context)
{
	DbgPrint("Terminator waiting 1 second ...");
	OSSleep(1000);
	
	DbgPrint("Terminating thread now...");
	OSTerminateThread((HANDLE) Context);
	
	DbgPrint("Terminating thread done.");
	OSExitThread();
}

void RunTest2()
{
	HANDLE Thread1, Thread2;
	BSTATUS Status;
	
	Status = OSCreateThread(&Thread1, CURRENT_PROCESS_HANDLE, NULL, ThreadTerminatee, NULL, true);
	if (FAILED(Status))
	{
		CRASH("Cannot create thread 1: %d (%s)", Status, RtlGetStatusString(Status));
		return;
	}
	
	Status = OSCreateThread(&Thread2, CURRENT_PROCESS_HANDLE, NULL, ThreadTerminator, (void*) Thread1, true);
	if (FAILED(Status))
	{
		CRASH("Cannot create thread 2: %d (%s)", Status, RtlGetStatusString(Status));
		OSClose(Thread1);
		return;
	}
	
	Status = OSSetSuspendedThread(Thread1, false);
	if (FAILED(Status))
	{
		CRASH("Cannot unsuspend thread 1: %d (%s)", Status, RtlGetStatusString(Status));
		OSClose(Thread1);
		OSClose(Thread2);
		return;
	}
	
	Status = OSSetSuspendedThread(Thread2, false);
	if (FAILED(Status))
	{
		CRASH("Cannot unsuspend thread 2: %d (%s)", Status, RtlGetStatusString(Status));
		OSClose(Thread1);
		OSClose(Thread2);
		return;
	}
	
	DbgPrint("Main thread waiting...");
	HANDLE Handles[2];
	Handles[0] = Thread1;
	Handles[1] = Thread2;
	
	Status = OSWaitForMultipleObjects(2, Handles, WAIT_ALL_OBJECTS, false, WAIT_TIMEOUT_INFINITE);
	if (FAILED(Status))
	{
		CRASH("Failed to wait for threads: %d (%s)", Status, RtlGetStatusString(Status));
		OSClose(Thread1);
		OSClose(Thread2);
		return;
	}
	
	DbgPrint("Closing handles.");
	OSClose(Thread1);
	OSClose(Thread2);
	
	DbgPrint("We're done!");
}
