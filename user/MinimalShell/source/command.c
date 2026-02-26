#include <boron.h>
#include <string.h>
#include "command.h"

#define MAX_COMMAND (8192)

/*
					The Boron Minimal Shell

	This file implements the base of the minimal shell. Currently, it
	only implements basic command syntax.  Since it's minimal, it does
	not, and will not, support advanced features like redirection or
	signal processing.  These will be supported by a more advanced
	shell which will be implemented later.
*/

static char CommandBuffer[MAX_COMMAND];

void CmdPrintPrompt()
{
	OSFPrintf(FILE_STANDARD_ERROR, "BMS> ");
}

void CmdPrintInitMessage()
{
	OSFPrintf(FILE_STANDARD_ERROR, "BORON Minimal Shell v0.0.1\n");
	OSFPrintf(FILE_STANDARD_ERROR, "Copyright (C) 2023 - 2026 iProgramInCpp\n\n");
}

void CmdReadInput()
{
	memset(CommandBuffer, 0, sizeof CommandBuffer);
	
	IO_STATUS_BLOCK Iosb;
	BSTATUS Status = OSReadFile(
		&Iosb,
		OSGetStandardInput(),
		0,    // ByteOffset
		CommandBuffer,
		sizeof(CommandBuffer) - 1,
		IO_RW_FINISH_ON_NEWLINE
	);
	
	if (IOFAILED(Status))
	{
		OSFPrintf(
			FILE_STANDARD_ERROR,
			"Could not read from standard input: %s\n",
			RtlGetStatusString(Status)
		);
		
		DbgPrint("Could not read from standard input: %s", RtlGetStatusString(Status));
		
		OSExitProcess(1);
	}
	
	size_t Length = Iosb.BytesRead;
	CommandBuffer[Length] = 0;
	
	// Strip newlines from the end.
	for (int i = (int)Length - 1; i >= 0; i--)
	{
		if (CommandBuffer[i] != '\n' && CommandBuffer[i] != '\r')
			break;
		
		CommandBuffer[i] = 0;
	}
}

static char CmdIsWhiteSpace(char Character)
{
	return Character == ' ' || Character == '\t' || Character == '\r' || Character == '\n';
}

static void CmdSkipWhiteSpace(char** Pointer)
{
	char* Pointer2 = *Pointer;
	
	while (CmdIsWhiteSpace(*Pointer2))
		Pointer2++;
	
	*Pointer = Pointer2;
}

static char CommandName[MAX_COMMAND], ArgumentBuffer[MAX_COMMAND];

static bool CmdParseWord(char** InputPtr, char** OutputPtr)
{
	char* CommandPtr = *InputPtr;
	char* WordPtr = *OutputPtr;
	
	while (*CommandPtr != 0 && !CmdIsWhiteSpace(*CommandPtr))
	{
		if (*CommandPtr == '"' || *CommandPtr == '\'')
		{
			// quote or apostrophe, meaning we must escape everything inside
			char Beginning = *CommandPtr;
			
			CommandPtr++;
			while (*CommandPtr != 0 && *CommandPtr != Beginning)
			{
				// NOTE TODO: we do not support multiple line concatenation via backslashing
				// before the newline, or putting unterminated quotes on the line.  therefore
				// unterminated quotes will result in an error.
				*WordPtr = *CommandPtr;
				WordPtr++;
				CommandPtr++;
			}
			
			if (*CommandPtr == 0)
			{
				OSFPrintf(FILE_STANDARD_ERROR, "ERROR: unterminated escape string\n");
				return false;
			}
			
			// terminated, so skip the termination character now.
			CommandPtr++;
			continue;
		}
		
		if (*CommandPtr == '\\')
		{
			// the next character will be placed verbatim in the word, unless it's 0
			CommandPtr++;
			
			if (*CommandPtr == 0)
			{
				OSFPrintf(FILE_STANDARD_ERROR, "ERROR: unterminated escape sequence\n");
				return false;
			}
		}
		
		// just a normal character
		*WordPtr = *CommandPtr;
		WordPtr++;
		CommandPtr++;
	}
	
	*WordPtr = 0;
	
	*InputPtr = CommandPtr;
	*OutputPtr = WordPtr;
	return true;
}

void CmdStartProcess(const char* CommandName, const char* ArgumentBuffer, bool Wait)
{
	// Create a new process and wait for it to finish.
	BSTATUS Status;
	HANDLE ProcessHandle;
	HANDLE MainThreadHandle;
	Status = OSCreateProcess(
		&ProcessHandle,
		&MainThreadHandle,
		NULL,
		OS_PROCESS_CMDLINE_PARSED,
		CommandName,
		ArgumentBuffer,
		NULL
	);
	
	if (FAILED(Status))
	{
		OSFPrintf(FILE_STANDARD_ERROR, "%s: %s\n", CommandName, RtlGetStatusString(Status));
		return;
	}
	
	// We have a process. Now wait for it if needed.
	if (Wait)
	{
		// TODO: Alertable = true
		Status = OSWaitForSingleObject(ProcessHandle, false, WAIT_TIMEOUT_INFINITE);
		
		if (FAILED(Status))
		{
			OSFPrintf(FILE_STANDARD_ERROR, "%s (while waiting for process): %s\n", CommandName, RtlGetStatusString(Status));
			return;
		}
	}
	// TODO: Otherwise, print information about the process run in the background.
	
	// Close both handles which will reap the process.
	OSClose(MainThreadHandle);
	OSClose(ProcessHandle);
}

void CmdParseCommand()
{
	memset(CommandName, 0, sizeof(CommandName));
	memset(ArgumentBuffer, 0, sizeof(ArgumentBuffer));
	
	char* CommandPtr = CommandBuffer;

	// Read command name and arguments.
	//
	// NOTE: We currently assume that the final argument buffer will be SHORTER than the total
	// length of the command.
	char* ArgumentBufferPtr = ArgumentBuffer;
	while (*CommandPtr != 0)
	{
		CmdSkipWhiteSpace(&CommandPtr);
		if (!CmdParseWord(&CommandPtr, &ArgumentBufferPtr))
			return;
		
		ArgumentBufferPtr++;
	}
	
	*ArgumentBufferPtr = 0;
	
	// The first argument (consistent with the way argv works on other platforms) is the
	// name of the desired command.
	const char* CommandName = ArgumentBuffer;
	if (*CommandName == 0) {
		DbgPrint("no command passed!");
		return;
	}
	
	// At this point, CommandName and ArgumentBuffer can just be used in an OSCreateProcess.
	// However, check for built-in commands first.
	if (CmdTryRunningBuiltInCommand(CommandName, ArgumentBuffer))
		return;
	
	CmdStartProcess(CommandName, ArgumentBuffer, true);
}

