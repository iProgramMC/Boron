#include <boron.h>

NO_RETURN
void TestThreadStart();

const char HiStr[] = "Hissssssssssss, Viper Lives\n";

HIDDEN
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
	OSWaitForSingleObject(Handle, false, WAIT_TIMEOUT_INFINITE);
	OSDummy();
	OSClose(Handle);
	OSDummy();
	OSExitThread();
}

NO_RETURN
void TestThreadStart()
{
	OSOutputDebugString(HiStr, sizeof HiStr);
	OSExitThread();
}
