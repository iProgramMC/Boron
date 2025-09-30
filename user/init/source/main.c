#include <boron.h>
#include <string.h>

extern void RunTest1();
extern void RunTest2();
extern void RunTest3();
extern void RunTest4();

int _start()
{
	DbgPrint("Init is running!\n");
	
	//RunTest1();
	//RunTest2();
	//RunTest3();
	//RunTest4();
	
	// try running test.exe
	const char* Path = "\\InitRoot\\test.exe";
	
	HANDLE Handle, ThreadHandle;
	BSTATUS Status = OSCreateProcess(
		&Handle,
		&ThreadHandle,
		NULL,
		true,
		false,
		Path,
		""
	);
	
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not start process %s. %s (%d)", Path, RtlGetStatusString(Status), Status);
		OSExitProcess(Status);
	}
	
	OSClose(ThreadHandle);
	DbgPrint("INIT: Process started successfully, waiting on it ...");
	
	Status = OSWaitForSingleObject(Handle, false, WAIT_TIMEOUT_INFINITE);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not wait on process %s. %s (%d)", Path, RtlGetStatusString(Status), Status);
		OSExitProcess(Status);
	}
	
	DbgPrint("INIT: Done! Closing and exiting.");
	OSClose(Handle);
	OSExitProcess(0);
}
