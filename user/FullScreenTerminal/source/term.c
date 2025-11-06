#include "terminal.h"

HANDLE TerminalHandle, TerminalHostHandle, TerminalSessionHandle;
HANDLE InputThread, OutputThread;

BSTATUS CreatePseudoterminal()
{
	BSTATUS Status = OSCreateTerminal(&TerminalHandle, NULL, DEFAULT_BUFFER_SIZE);
	if (FAILED(Status))
		return Status;
	
	Status = OSCreateTerminalIoHandles(&TerminalHostHandle, &TerminalSessionHandle, TerminalHandle);
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
	// TODO: Read from the keyboard and send data to the session.
	while (true)
		OSSleep(1000);
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
