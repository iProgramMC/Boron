#include "terminal.h"

IOCTL_FRAMEBUFFER_INFO FbInfo;
uint8_t* FbAddress;

BSTATUS UseFramebuffer(const char* FramebufferPath)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName = FramebufferPath;
	Attributes.ObjectNameLength = strlen(FramebufferPath);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	
	HANDLE FramebufferHandle;
	Status = OSOpenFile(&FramebufferHandle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Couldn't open framebuffer '%s'. %s (%d)", FramebufferPath, RtlGetStatusString(Status), Status);
		return Status;
	}
	
	Status = OSDeviceIoControl(
		FramebufferHandle,
		IOCTL_FRAMEBUFFER_GET_INFO,
		NULL,
		0,
		&FbInfo,
		sizeof FbInfo
	);
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Failed to get framebuffer info from %s. %s (%d)", FramebufferPath, RtlGetStatusString(Status), Status);
		OSClose(FramebufferHandle);
		return Status;
	}
	
	size_t Size = FbInfo.Pitch * FbInfo.Height;
	Status = OSMapViewOfObject(
		CURRENT_PROCESS_HANDLE,
		FramebufferHandle,
		(void**) &FbAddress,
		Size,
		MEM_COMMIT,
		0,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("ERROR: Failed to map %s. %s (%d)", FramebufferPath, RtlGetStatusString(Status), Status);
		OSClose(FramebufferHandle);
		return Status;
	}
	
	// don't need the handle to the file anymore, since we mapped it
	OSClose(FramebufferHandle);
	return STATUS_SUCCESS;
}
