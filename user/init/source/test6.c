#include <boron.h>
#include <string.h>
#include "misc.h"

void RunTest6()
{
	const char* Path = "/Devices/FrameBuffer0";
	
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = Path;
	Attributes.ObjectNameLength = strlen(Path);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	
	HANDLE Handle;
	BSTATUS Status = OSOpenFile(&Handle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to open %s. %s", Path, RtlGetStatusString(Status));
		return;
	}
	
	const int PITCH = 2048;
	const int HEIGHT = 800;
	
	void* BaseAddress = NULL;
	size_t Size = PITCH * HEIGHT * 4;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		Handle,
		&BaseAddress,
		Size,
		MEM_COMMIT,
		0,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to map %s. %s", Path, RtlGetStatusString(Status));
		OSClose(Handle);
		return;
	}
	
	uint32_t* Address = BaseAddress;
	for (int i = 0; i < 1024 * 768; i++)
	{
		*(Address++) = 0x12345678;
	}
}