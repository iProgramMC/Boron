#include <boron.h>
#include "misc.h"

void RunTest5()
{
	const char* Path = "test.exe";
	
	HANDLE Handle, ThreadHandle;
	BSTATUS Status = OSCreateProcess(
		&Handle,
		&ThreadHandle,
		NULL,
		true,
		false,
		Path,
		"",
		NULL
	);
	CHECK_STATUS(5);
	
	OSClose(ThreadHandle);
	DbgPrint("INIT: Process started successfully, waiting on it ...");
	
	Status = OSWaitForSingleObject(Handle, false, WAIT_TIMEOUT_INFINITE);
	CHECK_STATUS(5);
	
	DbgPrint("INIT: Done! Closing and exiting.");
	OSClose(Handle);
}