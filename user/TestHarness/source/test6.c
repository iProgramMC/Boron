#include <boron.h>
#include "testfmk.h"

void Test6ForkAndReplaceChildProcess()
{
	BSTATUS Status;
	HANDLE ChildHandle;
	
	OSPrintf("About to fork...\n");
	
	Status = OSForkProcess(&ChildHandle);
	if (Status == STATUS_IS_CHILD_PROCESS)
	{
		// We are the child process.
		OSPrintf("Hello from the child process...\n");
		
		Status = OSReplaceProcess(
			"/bin/Hello.exe",
			"",   // CommandLine
			NULL, // Environment
			NULL, // CurrentDirectory
			NULL, // Context
			0     // ContextSize
		);
		
		if (FAILED(Status)) {
			OSPrintf("Failed to launch /bin/Hello.exe: %s", RtlGetStatusString(Status));
		}
		
		OSExitProcess(1);
	}
	
	TestAssert(SUCCEEDED(Status));
	
	OSPrintf("Waiting for the child process to exit...\n");
	Status = OSWaitForSingleObject(ChildHandle, false, WAIT_TIMEOUT_INFINITE);
	TestAssert(SUCCEEDED(Status));
}
