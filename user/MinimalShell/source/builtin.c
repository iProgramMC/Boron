#include <boron.h>
#include <string.h>
#include "command.h"

typedef struct {
	const char* Command;
	void(*Handler)(const char*);
	const char* Description;
}
COMMAND_ENTRY;

// -- built-in commands start --

void CmdHelp();

void CmdExit(UNUSED const char* Arguments)
{
	OSExitProcess(1);
}

void CmdPrintImageName(UNUSED const char* Arguments)
{
	OSPrintf("Image name from PEB: '%s'\n", OSDLLGetCurrentPeb()->ImageName);
}

void CmdPrintArguments(UNUSED const char* CommandArguments)
{
	const char* Arguments = OSDLLGetCurrentPeb()->CommandLine;
	bool IsFirst = true;
	while (*Arguments)
	{
		OSPrintf(IsFirst ? "%s" : " %s", Arguments);
		Arguments += 1 + strlen(Arguments);
		IsFirst = false;
	}
	
	OSPrintf("\n");
}

void CmdPrintEnvironment(UNUSED const char* Arguments)
{
	const char* Environment = OSDLLGetCurrentPeb()->Environment;
	while (*Environment)
	{
		OSPrintf("%s\n", Environment);
		Environment += 1 + strlen(Environment);
	}
}

void CmdExecuteAndTime(const char* Arguments)
{
	// the first argument is of course 'time', so skip it
	Arguments += strlen(Arguments) + 1;
	
	// then start a new process
	uint64_t TicksStart, TicksEnd, Frequency;
	OSGetTickCount(&TicksStart);
	OSGetTickFrequency(&Frequency);
	
	CmdStartProcess(Arguments, Arguments, true);
	
	OSGetTickCount(&TicksEnd);
	
	// Calculate the difference in microseconds.  Frequency is ticks per second, so turn it into
	// ticks per microsecond by multiplying Difference by 1000000 and dividing by Frequency.
	//
	// This still shouldn't overflow because you wouldn't be running something here for years
	// on end, right?
	int64_t Difference = TicksEnd - TicksStart;
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Spent %lld ticks of real time, at a frequency of %lld ticks/s.\n",
		Difference,
		Frequency
	);
	
	Difference *= 1000000;
	Difference /= Frequency;
	
	OSFPrintf(
		FILE_STANDARD_ERROR,
		"Spent %lld.%06lld seconds of real time\n",
		Difference / 1000000,
		Difference % 1000000
	);
}

void CmdExecuteInBackground(const char* Arguments)
{
	// the first argument is of course 'bg', so skip it
	Arguments += strlen(Arguments) + 1;
	
	CmdStartProcess(Arguments, Arguments, false);
}

void CmdClearScreen(UNUSED const char* Arguments)
{
	OSPrintf("\x1B[3J\x1B[H\x1B[2J");
}

// -- built-in commands end --

#define ENTRY(name, func, desc) { name, func, desc }
COMMAND_ENTRY CommandTable[] = {
	ENTRY("help", CmdHelp, "Print this stuff"),
	ENTRY("?", CmdHelp, "Same as help"),
	ENTRY("exit", CmdExit, "Exits minimal shell"),
	ENTRY("imagename", CmdPrintImageName, "Prints image name from PEB"),
	ENTRY("arguments", CmdPrintArguments, "Prints arguments from PEB"),
	ENTRY("environment", CmdPrintEnvironment, "Prints environment from PEB"),
	ENTRY("bg", CmdExecuteInBackground, "Runs a command without waiting"),
	ENTRY("time", CmdExecuteAndTime, "Runs a command and prints the time it took"),
	ENTRY("clear", CmdClearScreen, "Clears the screen"),
};

void CmdHelp()
{
	OSPrintf("Minimal Shell built-in commands:\n\n");
	
	for (size_t i = 0; i < ARRAY_COUNT(CommandTable); i++)
	{
		char CommandName[32];
		strcpy(CommandName, CommandTable[i].Command);
		
		size_t Length = strlen(CommandName);
		while (Length < 20) {
			strcpy(CommandName + Length, " ");
			Length++;
		}
		
		OSPrintf("\t%s %s\n", CommandName, CommandTable[i].Description);
	}
}

bool CmdTryRunningBuiltInCommand(const char* CommandName, const char* Arguments)
{
	for (size_t i = 0; i < ARRAY_COUNT(CommandTable); i++)
	{
		if (strcmp(CommandTable[i].Command, CommandName) == 0)
		{
			CommandTable[i].Handler(Arguments);
			return true;
		}
	}
	
	return false;
}
