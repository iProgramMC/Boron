#include <boron.h>
#include <string.h>

IOCTL_FRAMEBUFFER_INFO FramebufferInfo;
void* FramebufferMapAddress;

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
	
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = FramebufferName;
	Attributes.ObjectNameLength = strlen(FramebufferName);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	
	HANDLE FramebufferHandle;
	Status = OSOpenFile(&FramebufferHandle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Couldn't open framebuffer '%s'. %s (%d)", FramebufferName, RtlGetStatusString(Status), Status);
		return 1;
	}
	
	Status = OSDeviceIoControl(
		FramebufferHandle,
		IOCTL_FRAMEBUFFER_GET_INFO,
		NULL,
		0,
		&FramebufferInfo,
		sizeof FramebufferInfo
	);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Failed to get framebuffer info from %s. %s (%d)", FramebufferName, RtlGetStatusString(Status), Status);
		OSClose(FramebufferHandle);
		return 1;
	}
	
	size_t Size = FramebufferInfo.Pitch * FramebufferInfo.Height;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FramebufferHandle,
		&FramebufferMapAddress,
		Size,
		MEM_COMMIT,
		0,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Failed to map %s. %s (%d)", FramebufferName, RtlGetStatusString(Status), Status);
		OSClose(FramebufferHandle);
		return 1;
	}
	
	
	return 0;
}
