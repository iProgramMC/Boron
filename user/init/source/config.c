// 9 November 2025 - iProgramInCpp
#include <boron.h>
#include <string.h>
#include "config.h"

BSTATUS LoadConfigFile(const char* File)
{
	BSTATUS Status;
	HANDLE Handle;
	OBJECT_ATTRIBUTES Attributes;
	
	Attributes.ObjectName = File;
	Attributes.ObjectNameLength = strlen(File);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = HANDLE_NONE;
	Status = OSOpenFile(&Handle, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("Init: Failed to open config file '%s'. %s", File, RtlGetStatusString(Status));
		return Status;
	}
	
	uint64_t FileLength = 0;
	Status = OSGetLengthFile(Handle, &FileLength);
	if (FAILED(Status))
	{
		DbgPrint("Init: Failed to get size of config file '%s'. %s", File, RtlGetStatusString(Status));
		OSClose(Handle);
		return Status;
	}
	
	char* Data = OSAllocate(FileLength);
	if (!Data)
	{
		DbgPrint("Init: Failed to allocate space to read config file.");
		OSClose(Handle);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	IO_STATUS_BLOCK Iosb;
	Status = OSReadFile(&Iosb, Handle, 0, Data, FileLength, 0);
	if (FAILED(Status) || Iosb.BytesRead != FileLength)
	{
		DbgPrint("Init: Failed to read config file '%s' (read %zu bytes). %s", File, Iosb.BytesRead, RtlGetStatusString(Status));
		OSClose(Handle);
		return Status;
	}
	
	OSClose(Handle);
	
	return STATUS_SUCCESS;
}
