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

void CmdExecuteAndTime(const char* FullArguments)
{
	if (*FullArguments == 0)
	{
		OSFPrintf(FILE_STANDARD_ERROR, "No command provided\n");
		return;
	}
	
	// then start a new process
	uint64_t TicksStart, TicksEnd, Frequency;
	OSGetTickCount(&TicksStart);
	OSGetTickFrequency(&Frequency);
	
	const char* CommandName = FullArguments;
	const char* Arguments = FullArguments + strlen(FullArguments) + 1;
	
	CmdStartProcess(CommandName, Arguments, true);
	
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

void CmdExecuteAsync(const char* FullArguments)
{
	if (*FullArguments == 0)
	{
		OSFPrintf(FILE_STANDARD_ERROR, "No command provided\n");
		return;
	}
	
	const char* CommandName = FullArguments;
	const char* Arguments = FullArguments + strlen(FullArguments) + 1;
	
	CmdStartProcess(CommandName, Arguments, false);
}

void CmdClearScreen(UNUSED const char* Arguments)
{
	OSPrintf("\x1B[3J\x1B[H\x1B[2J");
}

void CmdSystemInfoBasic()
{
	size_t WrittenSize;
	SYSTEM_BASIC_INFORMATION BasicInfo;
	BSTATUS Status = OSQuerySystemInformation(
		QUERY_BASIC_INFORMATION,
		&BasicInfo,
		sizeof BasicInfo,
		&WrittenSize
	);
	
	if (FAILED(Status)) {
		OSFPrintf(FILE_STANDARD_ERROR, "Could not get system info: %s", RtlGetStatusString(Status));
		return;
	}
	
	int Vn = BasicInfo.OSVersionNumber;
	
	OSPrintf("OS Name:                   %s\n", BasicInfo.OSName);
	OSPrintf("OS Version:                v%u.%u.%u\n", VER_MAJOR(Vn), VER_MINOR(Vn), VER_BUILD(Vn));
	OSPrintf("Page Size:                 %u\n", BasicInfo.PageSize);
	OSPrintf("Allocation Granularity:    %u\n", BasicInfo.AllocationGranularity);
	OSPrintf("Processor Count:           %u\n", BasicInfo.ProcessorCount);
	OSPrintf("Minimum User Mode Address: %p\n", BasicInfo.MinimumUserModeAddress);
	OSPrintf("Maximum User Mode Address: %p\n", BasicInfo.MaximumUserModeAddress);
}

void CmdSystemInfoMemory()
{
	size_t WrittenSize;
	SYSTEM_MEMORY_INFORMATION MemoryInfo;
	BSTATUS Status = OSQuerySystemInformation(
		QUERY_MEMORY_INFORMATION,
		&MemoryInfo,
		sizeof MemoryInfo,
		&WrittenSize
	);
	
	if (FAILED(Status)) {
		OSFPrintf(FILE_STANDARD_ERROR, "Could not get memory info: %s", RtlGetStatusString(Status));
		return;
	}
	
	//OSPrintf("Page Size:             %u\n", MemoryInfo.PageSize);
	//OSPrintf("Total Physical Memory: %zu KB\n", MemoryInfo.TotalPhysicalMemoryPages * MemoryInfo.PageSize / 1024);
	OSPrintf("Free Physical Memory:  %zu KB\n", MemoryInfo.FreePhysicalMemoryPages * MemoryInfo.PageSize / 1024);
}

void CmdSystemInfoMemory2() {
	while (true) {
		CmdSystemInfoMemory();
	}
}

void CmdSystemInfoProcess()
{
	OSPrintf("TODO\n");
}

// -- built-in commands end --

#define ENTRY(name, func, desc) { name, func, desc }
COMMAND_ENTRY CommandTable[] = {
	ENTRY("help",  CmdHelp, "Print this stuff"),
	ENTRY("?",     CmdHelp, "Same as help"),
	ENTRY("async", CmdExecuteAsync, "Start async process"),
	ENTRY("&",     CmdExecuteAsync, "Same as async"),
	ENTRY("args",  CmdPrintArguments, "Print arguments from PEB"),
	ENTRY("clear", CmdClearScreen, "Clear terminal display"),
	ENTRY("env",   CmdPrintEnvironment, "Print environment from PEB"),
	ENTRY("exit",  CmdExit, "Exits minimal shell"),
	ENTRY("iname", CmdPrintImageName, "Print image name from PEB"),
	ENTRY("time",  CmdExecuteAndTime, "Start process and print execution time"),
	ENTRY("bi",    CmdSystemInfoBasic, "Get basic system info"),
	ENTRY("mi",    CmdSystemInfoMemory, "Get system memory info"),
	ENTRY("mi2",    CmdSystemInfoMemory2, "Get system memory info"),
	ENTRY("ps",    CmdSystemInfoProcess, "Get system process info"),
};

void CmdHelp()
{
	OSPrintf("Minimal Shell built-in commands:\n\n");
	
	for (size_t i = 0; i < ARRAY_COUNT(CommandTable); i++)
	{
		char CommandName[32];
		strcpy(CommandName, CommandTable[i].Command);
		
		size_t Length = strlen(CommandName);
		while (Length < 10) {
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
