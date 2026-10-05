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

BSTATUS CmdChangeDirectory(const char* PathName)
{
	BSTATUS Status;
	char TempBuffer[512];
	
	if (PathName[0] == '/') {
		// Path name is absolute, just forward it
		return OSSetCurrentDirectory(PathName);
	}
	
	// Path name is relative, therefore we need to adjust it
	Status = OSGetCurrentDirectoryPath(TempBuffer, sizeof(TempBuffer));
	
	if (FAILED(Status))
		return Status;
	
	const char* PathNamePtr = PathName;
	while (*PathNamePtr)
	{
		DbgPrint("TempBuf is now '%s', PathNamePtr is now %s", TempBuffer, PathNamePtr);
		
		if (*PathNamePtr == '/') {
			PathNamePtr++;
			continue;
		}
		
		bool IsDotEnd = PathNamePtr[0] == '.' && PathNamePtr[1] == 0;
		bool IsDotSlash = PathNamePtr[0] == '.' && PathNamePtr[1] == '/';
		
		if (IsDotEnd || IsDotSlash)
		{
			// ".", so ignore the path component
			PathNamePtr++;
			
			if (IsDotSlash)
				PathNamePtr++;
			
			continue;
		}
		
		bool IsDotDotEnd = PathNamePtr[0] == '.' && PathNamePtr[1] == '.' && PathNamePtr[2] == 0;
		bool IsDotDotSlash = PathNamePtr[0] == '.' && PathNamePtr[1] == '.' && PathNamePtr[2] == '/';
		
		if (IsDotDotEnd || IsDotDotSlash)
		{
			// Pop a path component from the end of the temporary buffer.
			int Length = (int) strlen(TempBuffer);
			for (int i = Length - 1; i >= 0; i--)
			{
				if (TempBuffer[i] == '/')
				{
					TempBuffer[i] = 0;
					if (i == 0) {
						strcpy(TempBuffer, "/");
					}
					break;
				}
			}
			
			PathNamePtr += 2;
			if (IsDotDotSlash)
				PathNamePtr++;
			
			continue;
		}
		
		// Regular path component: append it to the current directory.
		size_t CompLength = 0;
		for (; PathNamePtr[CompLength] != 0 && PathNamePtr[CompLength] != '/'; CompLength++);
		
		bool IsAtRoot = strcmp(TempBuffer, "/") == 0;
		size_t TempBufLength = strlen(TempBuffer);
		if (TempBufLength + (IsAtRoot ? 1 : 0) + CompLength + 1 > sizeof(TempBuffer))
		{
			DbgPrint("CmdChangeDirectory: Buffer overflow while trying to parse path '%s'", PathName);
			return STATUS_NAME_TOO_LONG;
		}
		
		if (!IsAtRoot) {
			strcpy(TempBuffer + TempBufLength, "/");
			TempBufLength++;
		}
		
		memcpy(TempBuffer + TempBufLength, PathNamePtr, CompLength);
		TempBuffer[TempBufLength + CompLength] = 0;
		
		PathNamePtr += CompLength;
		if (*PathNamePtr == '/')
			PathNamePtr++;
	}
	
	DbgPrint("END: TempBuf is now '%s'", TempBuffer);
	return OSSetCurrentDirectory(TempBuffer);
}

void CmdChangeDir(const char* FullArguments)
{
	// TODO: take to $HOME instead
	const char* DestinationDirectory = "/";
	
	if (*FullArguments != 0)
	{
		DestinationDirectory = FullArguments;
	}
	
	BSTATUS Status = CmdChangeDirectory(DestinationDirectory);
	if (FAILED(Status))
	{
		OSPrintf("cd: %s: %s\n", DestinationDirectory, RtlGetStatusString(Status));
	}
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
	
	OSPrintf("Page Size:             %u\n", MemoryInfo.PageSize);
	OSPrintf("Total Physical Memory: %zu KB\n", MemoryInfo.TotalPhysicalMemoryPages * MemoryInfo.PageSize / 1024);
	OSPrintf("Free Physical Memory:  %zu KB\n", MemoryInfo.FreePhysicalMemoryPages * MemoryInfo.PageSize / 1024);
}

void CmdTest1()
{
	while (true)
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
		
		OSPrintf("Free Physical Memory:  %zu KB\n", MemoryInfo.FreePhysicalMemoryPages * MemoryInfo.PageSize / 1024);
	}
}

void CmdSystemInfoProcess()
{
	OSPrintf("TODO\n");
}

void CmdShutDown()
{
	BSTATUS Status = OSShutDownSystem();
	
	if (FAILED(Status))
	{
		OSPrintf("Shutdown failed: %s\n", RtlGetStatusString(Status));
		return;
	}
	
	OSPrintf("Should be shutting down soon...\n");
}

void CmdDebugTest();

void CmdDebugTest2()
{
	for (int i = 0; i < 8; i++) {
		CmdDebugTest();
	}
}

// -- built-in commands end --

#define ENTRY(name, func, desc) { name, func, desc }
COMMAND_ENTRY CommandTable[] = {
	ENTRY("help",     CmdHelp, "Print this stuff"),
	ENTRY("?",        CmdHelp, "Same as help"),
	ENTRY("cd",       CmdChangeDir, "Change working directory"),
	ENTRY("chdir",    CmdChangeDir, "Same as cd"),
	ENTRY("async",    CmdExecuteAsync, "Start async process"),
	ENTRY("&",        CmdExecuteAsync, "Same as async"),
	ENTRY("args",     CmdPrintArguments, "Print arguments from PEB"),
	ENTRY("clear",    CmdClearScreen, "Clear terminal display"),
	ENTRY("env",      CmdPrintEnvironment, "Print environment from PEB"),
	ENTRY("exit",     CmdExit, "Exits minimal shell"),
	ENTRY("iname",    CmdPrintImageName, "Print image name from PEB"),
	ENTRY("time",     CmdExecuteAndTime, "Start process and print execution time"),
	ENTRY("bi",       CmdSystemInfoBasic, "Get basic system info"),
	ENTRY("mi",       CmdSystemInfoMemory, "Get system memory info"),
	ENTRY("ps",       CmdSystemInfoProcess, "Get system process info"),
	ENTRY("test1",    CmdTest1, "Run the 'free memory' command in a loop"),
	ENTRY("shutdown", CmdShutDown, "Shuts down the system"),
	ENTRY("d",        CmdDebugTest, "Runs Hello.exe then reports memory usage (Hello && mi)"),
	ENTRY("d2",       CmdDebugTest2, "Runs Hello.exe then reports memory usage 8 times (Hello && mi)"),
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
