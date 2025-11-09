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

void Usage()
{
	DbgPrint("FullScreenTerminal - help");
	DbgPrint("	--framebuffer [fileName]: The frame buffer device to which this terminal instance will output.");
	
	// TODO: implement these ...
	DbgPrint("	--keyboard [fileName]:    The keyboard from which this terminal instance will read user input.");
	DbgPrint("	--frameless:              Do not render a frame around the terminal.");
	DbgPrint("	--title [title]:          The terminal frame's title.  Ignored if '--frameless' is specified.");
}

int _start(int ArgumentCount, char** ArgumentArray)
{
	BSTATUS Status;
	
	char* FramebufferName = NULL;
	
	for (int i = 1; i < ArgumentCount; i++)
	{
		if (strcmp(ArgumentArray[i], "--framebuffer") == 0)
			FramebufferName = ArgumentArray[i + 1];
	}
	
	if (!FramebufferName)
	{
		DbgPrint("ERROR: No framebuffer specified.");
		Usage();
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
	
	const char* InteractiveShellName = OSDLLGetEnvironmentVariable("INTERACTIVE_SHELL");
	const char* InteractiveShellArgs = OSDLLGetEnvironmentVariable("INTERACTIVE_SHELL_ARGS");
	
	if (InteractiveShellName)
	{
		Status = LaunchProcess(InteractiveShellName, InteractiveShellArgs);
		if (FAILED(Status))
		{
			OSPrintf("ERROR: Could not run '%s'. %s\n", InteractiveShellName, RtlGetStatusString(Status));
			DbgPrint("ERROR: Could not run '%s'. %s", InteractiveShellName, RtlGetStatusString(Status));
		}
	}
	else
	{
		OSPrintf("ERROR: Interactive shell name not specified.\n");
		DbgPrint("ERROR: Interactive shell name not specified.");
	}
	
	while (true)
	{
		Status = WaitOnIOLoopThreads();
		if (FAILED(Status))
			DbgPrint("ERROR: Could not wait on I/O loop threads. %s", RtlGetStatusString(Status));
	}
	
	return 0;
}
