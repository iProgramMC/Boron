#include <boron.h>

NO_RETURN
void TestThreadStart();

HIDDEN
void DLLEntryPoint()
{
	HANDLE Handle;
	BSTATUS Status;
	
	DbgPrint("Calling OSDummy twice.");
	OSDummy();
	OSDummy();
	
	// create a new thread for fun
	DbgPrint("Creating new thread.");
	Status = OSCreateThread(
		&Handle,
		CURRENT_PROCESS_HANDLE,
		NULL,
		TestThreadStart,
		NULL,
		false
	);
	
	DbgPrint("Got handle %lld, status %d from OSCreateThread: %s", (long long) Handle, Status, RtlGetStatusString(Status));
	
	DbgPrint("Waiting on new thread handle.");
	OSWaitForSingleObject(Handle, false, WAIT_TIMEOUT_INFINITE);
	
	DbgPrint("Closing new thread handle.");
	OSClose(Handle);
	
	DbgPrint("Creating a new empty process, then closing it.");
	
	Status = OSCreateProcess(
		&Handle,
		NULL,
		CURRENT_PROCESS_HANDLE,
		false
	);
	
	DbgPrint("Got handle %lld, status %d from OSCreateProcess: %s", (long long) Handle, Status, RtlGetStatusString(Status));
	
	DbgPrint("Closing new thread handle.");
	OSClose(Handle);
	
	OSDummy();
	
	DbgPrint("Main Thread exiting.");
	OSExitThread();
}

NO_RETURN
void TestThreadStart()
{
	OSDummy();
	
	DbgPrint("New Thread exiting.");
	OSExitThread();
}
