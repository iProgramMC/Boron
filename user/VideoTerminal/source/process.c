#include "terminal.h"

static HANDLE ProcessHandle;

BSTATUS LaunchProcess(const char* FileName, const char* Arguments)
{
	if (!FileName)
		return STATUS_INVALID_PARAMETER;
	
	// Now creating the process should also give it the I/O handles.
	// TODO: Currently just test.exe
	HANDLE MainThreadHandle;
	BSTATUS Status = OSCreateProcess(
		&ProcessHandle,
		&MainThreadHandle,
		NULL, // ObjectAttributes
		0,    // Flags
		FileName,
		Arguments,
		NULL  // Environment
	);
	
	if (SUCCEEDED(Status))
		OSClose(MainThreadHandle);
	
	return Status;
}
