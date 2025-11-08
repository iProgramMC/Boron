#include "terminal.h"

static HANDLE ProcessHandle;

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
		NULL, // ObjectAttributes
		0,    // Flags
		"test.exe",
		"",   // CommandLine
		NULL  // Environment
	);
	
	return Status;
}
