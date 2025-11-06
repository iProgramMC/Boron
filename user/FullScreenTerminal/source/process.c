#include "terminal.h"

static HANDLE ThreadHandle;

NO_RETURN
void ProducerThread(UNUSED void* Context)
{
	int i = 1;
	while (true)
	{
		OSSleep(1000);
		
		char Buffer[1024];
		snprintf(
			Buffer,
			sizeof Buffer,
			"\nHi there!  This message still came from FullScreenTerminal.exe, but this time it went "
			"through the terminal object!  This is iteration # %d.\n",
			i++
		);
		
		IO_STATUS_BLOCK Iosb;
		BSTATUS Status = OSWriteFile(
			&Iosb,
			TerminalSessionHandle,
			0,   // FileOffset
			Buffer,
			strlen(Buffer),
			0,   // Flags
			NULL // OutFileSize
		);
		
		if (IOFAILED(Status))
		{
			DbgPrint("ProducerThread: Output failed. %s (%d)", RtlGetStatusString(Status), Status);
			continue;
		}
		
		if (Status != STATUS_SUCCESS)
		{
			DbgPrint("ProducerThread: Output returned early. %s (%d)", RtlGetStatusString(Status), Status);
			continue;
		}
	}
}

BSTATUS LaunchProcess(const char* CommandLine)
{
	// TODO: Actually launch a process.
	(void) CommandLine;
	
	BSTATUS Status = OSCreateThread(
		&ThreadHandle,
		CURRENT_PROCESS_HANDLE,
		NULL, // ObjectAttributes
		ProducerThread,
		NULL, // ThreadContext
		false // CreateSuspended
	);
	
	return Status;
}
