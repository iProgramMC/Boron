#include <boron.h>
#include <string.h>

extern void RunTest1();
extern void RunTest2();
extern void RunTest3();
extern void RunTest4();
extern void RunTest5();
extern void RunTest6();

int _start(int ArgumentCount, char** Arguments)
{
	DbgPrint("Init is running.");
	
	// TODO: Load environment variables from a config file
	(void) ArgumentCount;
	(void) Arguments;
	
	// Create the terminal process.
	
	// TODO: Allow specification of the framebuffer again.
	
	HANDLE ProcessHandle, ThreadHandle;
	BSTATUS Status = OSCreateProcess(
		&ProcessHandle,
		&ThreadHandle,
		NULL,  // ObjectAttributes
		0,     // ProcessFlags
		"FullScreenTerminal.exe",
		"--framebuffer /Devices/FrameBuffer0",
		NULL   // Environment
	);
	
	if (FAILED(Status))
	{
		DbgPrint(
			"Init: Could not launch FullScreenTerminal.exe: %s (%d)",
			RtlGetStatusString(Status),
			Status
		);
		
		return Status;
	}
	
	OSClose(ThreadHandle);
	
	Status = OSWaitForSingleObject(ProcessHandle, false, WAIT_TIMEOUT_INFINITE);
	OSClose(ProcessHandle);
	
	OSExitProcess(0);
}
