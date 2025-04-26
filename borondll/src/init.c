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
	BSTATUS Status = OSCreateThread(
		&Handle,
		CURRENT_PROCESS_HANDLE,
		NULL,
		TestThreadStart,
		NULL,
		false
	);
	
	DbgPrint("Status: %d  (%p)", Status, RtlGetStatusString(Status));
	
	OSDummy();
	DbgPrint("Waiting...");
	OSDummy();
	OSWaitForSingleObject(Handle, false, WAIT_TIMEOUT_INFINITE);
	OSDummy();
	DbgPrint("Waiting Done...");
	OSDummy();
	OSClose(Handle);
	OSDummy();
	OSExitThread();
}

NO_RETURN
void TestThreadStart()
{
	OSDummy();
	OSOutputDebugString(HiStr, sizeof HiStr);
	OSDummy();
	OSExitThread();
}
