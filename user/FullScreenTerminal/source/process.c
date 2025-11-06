#include "terminal.h"

static HANDLE ProcessHandle;

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
	// First of all, we should give ourselves the handle to the session
	// for standard IO.
	PPEB Peb = OSGetCurrentPeb();
	
	Peb->StandardIO[0] = TerminalSessionHandle;
	Peb->StandardIO[1] = TerminalSessionHandle;
	Peb->StandardIO[2] = TerminalSessionHandle;
	
	OSPrintf("Well, this is still from FullScreenTerminal.exe, but whatever.\n\n");
	
	// Now creating the process should also give it the I/O handles.
	// TODO: Currently just test.exe
	HANDLE MainThreadHandle;
	BSTATUS Status = OSCreateProcess(
		&ProcessHandle,
		&MainThreadHandle,
		NULL,
		false,
		false,
		"test.exe",
		"",
		NULL
	);
	
	return Status;
}
