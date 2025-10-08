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
	
	IOCTL_FRAMEBUFFER_INFO Info;
	Status = OSDeviceIoControl(
		Handle,
		IOCTL_FRAMEBUFFER_GET_INFO,
		NULL,
		0,
		&Info,
		sizeof Info
	);
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to get framebuffer info from %s. %s", Path, RtlGetStatusString(Status));
		OSClose(Handle);
		return;
	}
	
	void* BaseAddress = NULL;
	size_t Size = Info.Pitch * Info.Height;
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

	uint32_t RowStart = 0;
	
	uint32_t Thr1 = Info.Height * 1 / 5;
	uint32_t Thr2 = Info.Height * 2 / 5;
	uint32_t Thr3 = Info.Height * 3 / 5;
	uint32_t Thr4 = Info.Height * 4 / 5;
	
	for (uint32_t i = 0; i < Info.Height; i++)
	{
		uint32_t* Row = (uint32_t*)((uint8_t*)BaseAddress + RowStart);
		
		for (uint32_t j = 0; j < Info.Width; j++)
		{
			uint32_t Color = 0xFFFFFF;
			if (i < Thr1 || i > Thr4)
				Color = 0x00FFFF;
			else if (i < Thr2 || i > Thr3)
				Color = 0xDF9AFF;
			
			Row[j] = Color;
		}
		
		RowStart += Info.Pitch;
	}
	
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		BaseAddress,
		Size,
		MEM_RELEASE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Test6: Failed to unmap %s. %s", Path, RtlGetStatusString(Status));
		OSClose(Handle);
		return;
	}
	
	OSClose(Handle);
}