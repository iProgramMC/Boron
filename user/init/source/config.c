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
	
	OSClose(Handle);
	return STATUS_SUCCESS;
}
