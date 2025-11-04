#include <boron.h>
#include <string.h>

extern void RunTest1();
extern void RunTest2();
extern void RunTest3();
extern void RunTest4();
extern void RunTest5();
extern void RunTest6();

int _start()
{
	DbgPrint("Init is running!\n");
	
	//RunTest1();
	//RunTest2();
	//RunTest3();
	//RunTest4();
	//RunTest5();
	//RunTest6();
	
	// TODO: Load environment variables from a config file
	
	// Create the terminal process.
	char TerminalCmdLine[IO_MAX_NAME + 200];
	snprintf(TerminalCmdLine, sizeof TerminalCmdLine, "--framebuffer=%s", OSDLLGetEnvironmentVariable("TerminalFramebuffer"));
	
	HANDLE ProcessHandle, ThreadHandle;
	BSTATUS Status = OSCreateProcess(
		&ProcessHandle,
		&ThreadHandle,
		NULL,  // ObjectAttributes
		false, // InheritHandles
		false, // CreateSuspended
		"FullScreenTerminal.exe",
		TerminalCmdLine,
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
