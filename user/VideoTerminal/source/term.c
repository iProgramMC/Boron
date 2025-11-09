#include "terminal.h"

#define MAX_LINE_BUFFER (8192)

HANDLE TerminalHandle, TerminalHostHandle, TerminalSessionHandle;
HANDLE InputThread, OutputThread;

BSTATUS CreatePseudoterminal()
{
	BSTATUS Status = OSCreateTerminal(&TerminalHandle, NULL, DEFAULT_BUFFER_SIZE);
	if (FAILED(Status))
		return Status;
	
	Status = OSCreateTerminalIoHandles(&TerminalHostHandle, &TerminalSessionHandle, TerminalHandle);
	if (FAILED(Status))
		return Status;
	
	// Give ourselves the handle to the session for standard IO.
	PPEB Peb = OSGetCurrentPeb();
	
	Peb->StandardIO[0] = TerminalSessionHandle;
	Peb->StandardIO[1] = TerminalSessionHandle;
	Peb->StandardIO[2] = TerminalSessionHandle;
	
	return Status;
}

NO_RETURN
void InputLoop(UNUSED void* Context)
{
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	char Buffer[1024];
	
	while (true)
	{
		Status = OSReadFile(
			&Iosb,
			TerminalHostHandle,
			0,
			Buffer,
			sizeof Buffer,
			IO_RW_NONBLOCK_UNLESS_EMPTY
		);
		
		if (IOFAILED(Status))
		{
			DbgPrint("Reading from session failed. %s (%d)", RtlGetStatusString(Status), Status);
			continue;
		}
		
		if (Iosb.BytesRead == 0)
		{
			DbgPrint("Read 0 bytes!");
			continue;
		}
		
		TerminalWrite(Buffer, Iosb.BytesRead);
		Iosb.BytesRead = 0;
	}
}

NO_RETURN
void OutputLoop(UNUSED void* Context)
{
	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	char Buffer[16];
	
	char LineBuffer[MAX_LINE_BUFFER];
	
	while (true)
	{
		Status = OSReadFile(
			&Iosb,
			KeyboardHandle,
			0,
			Buffer,
			sizeof Buffer,
			IO_RW_NONBLOCK_UNLESS_EMPTY
		);
		
		if (IOFAILED(Status))
		{
			DbgPrint("Reading from keyboard failed. %s (%d)", RtlGetStatusString(Status), Status);
			continue;
		}
		
		if (Iosb.BytesRead == 0)
		{
			DbgPrint("Read 0 bytes!");
			continue;
		}
		
		size_t WriteCount = 0;
		for (size_t i = 0; i < Iosb.BytesRead; i++)
		{
			LineBuffer[WriteCount] = TranslateKeyCode(Buffer[i]);
			if (LineBuffer[WriteCount])
				WriteCount++;
		}
		
		// If there's nothing to write, don't even bother doing the system call.
		if (!WriteCount)
			continue;
		
		Status = OSWriteFile(
			&Iosb,
			TerminalHostHandle,
			0,    // ByteOffset
			LineBuffer,
			WriteCount,
			0,    // Flags
			NULL  // OutFileSize
		);
		
		if (IOFAILED(Status))
		{
			DbgPrint("Writing to session failed. %s (%d)", RtlGetStatusString(Status), Status);
			continue;
		}
	}
}

BSTATUS CreateIOLoopThreads()
{
	BSTATUS Status = OSCreateThread(
		&InputThread,
		CURRENT_PROCESS_HANDLE,
		NULL, // ObjectAttributes
		InputLoop,
		NULL, // ThreadContext
		false // CreateSuspended
	);
	if (FAILED(Status))
		return Status;
	
	Status = OSCreateThread(
		&OutputThread,
		CURRENT_PROCESS_HANDLE,
		NULL, // ObjectAttributes,
		OutputLoop,
		NULL, // ThreadContext
		false // CreateSuspended
	);
	return Status;
}

BSTATUS WaitOnIOLoopThreads()
{
	HANDLE Handles[2];
	Handles[0] = InputThread;
	Handles[1] = OutputThread;
	
	return OSWaitForMultipleObjects(2, Handles, WAIT_ALL_OBJECTS, false, WAIT_TIMEOUT_INFINITE);
}
