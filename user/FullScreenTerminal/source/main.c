#include "terminal.h"

bool GetCommandLineArgument(const char* Key, char* ValueOut, size_t ValueOutSize)
{
	PPEB Peb = OSDLLGetCurrentPeb();
	const char* Place = strstr(Peb->CommandLine, Key);
	if (!Place)
		return false;
	
	const char* End = strchr(Place, ' ');
	if (!End)
		End = Place + strlen(Place);
	
	size_t Length = End - Place;
	if (Length >= ValueOutSize - 1)
		Length =  ValueOutSize - 1;
	
	memcpy(ValueOut, Place + strlen(Key), Length);
	ValueOut[Length] = 0;
	return true;
}

int _start()
{
	BSTATUS Status;
	
	char FramebufferName[IO_MAX_NAME];
	if (!GetCommandLineArgument("--framebuffer=", FramebufferName, sizeof(FramebufferName)))
	{
		DbgPrint("ERROR: No framebuffer specified.  Use the command line `--framebuffer=[framebuffer device here]`.");
		return 1;
	}
	
	Status = UseFramebuffer(FramebufferName);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Frame buffer '%s' couldn't be used. %s", FramebufferName, RtlGetStatusString(Status));
		return 1;
	}
	
	Status = SetupTerminal();
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not setup terminal. %s", RtlGetStatusString(Status));
		return 1;
	}
	
	Status = CreatePseudoterminal();
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not create pseudoterminal. %s", RtlGetStatusString(Status));
		return 1;
	}
	
	Status = CreateIOLoopThreads();
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not CreateIOLoopThreads. %s", RtlGetStatusString(Status));
		return 1;
	}
	
	const char* Arguments = OSDLLGetCurrentPeb()->CommandLine;
	Status = LaunchProcess(Arguments);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Could not create process according to arguments '%s'. %s", Arguments, RtlGetStatusString(Status));
		return 1;
	}
	
	while (true)
	{
		Status = WaitOnIOLoopThreads();
		if (FAILED(Status))
			DbgPrint("ERROR: Could not wait on I/O loop threads. %s", RtlGetStatusString(Status));
	}
	
	return 0;
}
