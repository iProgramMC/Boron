#include <boron.h>
#include <string.h>
#include "config.h"

void Usage()
{
	DbgPrint("Init - Help");
	DbgPrint("	--config [fileName]: System configuration file");
	OSExitProcess(1);
}

int _start(int ArgumentCount, char** Arguments)
{
	BSTATUS Status;
	DbgPrint("Init is running.");
	
	if (ArgumentCount <= 1)
		Usage();
	
	char* ConfigFile = NULL;
	for (int i = 1; i < ArgumentCount; i++)
	{
		if (strcmp(Arguments[i], "--config") == 0)
			ConfigFile = Arguments[i + 1];
	}
	
	if (!ConfigFile)
		Usage();
	
	Status = InitLoadConfigFile(ConfigFile);
	
	// Create the terminal process.
	
	// TODO: Allow specification of the framebuffer again.
	
	HANDLE ProcessHandle, ThreadHandle;
	Status = OSCreateProcess(
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
