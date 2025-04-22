#include <boron.h>
#include "lib.h"

NO_RETURN
void TestThreadStart();

void DLLEntryPoint()
{
	OSDummy();
	OSDummy();
	
	// create a new thread for fun
	HANDLE Handle;
	OSCreateThread(
		&Handle,
		CURRENT_PROCESS_HANDLE,
		NULL,
		TestThreadStart,
		NULL,
		false
	);
	
	OSDummy();
	OSWaitForSingleObject(Handle, false, TIMEOUT_INFINITE);
	OSDummy();
	OSClose(Handle);
	OSDummy();
	OSExitThread();
}

NO_RETURN
void TestThreadStart()
{
	OSDummy();
	OSDummy();
	OSDummy();
	OSExitThread();
}
