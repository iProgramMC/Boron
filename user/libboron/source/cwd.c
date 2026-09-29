#include <boron.h>
#include <rtl/assert.h>
#include "pebteb.h"

HANDLE OSGetCurrentDirectory()
{
	return OSDLLGetCurrentPeb()->CurrentDirectory.Handle;
}

BSTATUS OSSetCurrentDirectory(const char* PathName)
{
	if (!PathName) {
		return STATUS_INVALID_PARAMETER;
	}
	
	size_t PathNameLength = strlen(PathName);
	if (PathNameLength == 0) {
		return STATUS_INVALID_PARAMETER;
	}
	
	char* PathNameCopied = OSAllocate(PathNameLength + 1);
	if (!PathNameCopied)
	{
		DbgPrint("OSSetCurrentDirectory failed because out of memory!");
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	strcpy(PathNameCopied, PathName);
	
	HANDLE NewDirectory;
	OBJECT_ATTRIBUTES Attributes;
	OSInitializeObjectAttributes(&Attributes);
	OSSetNameObjectAttributes(&Attributes, PathName);
	
	BSTATUS Status = OSOpenFile(&NewDirectory, &Attributes);
	if (FAILED(Status))
	{
		DbgPrint("OSSetCurrentDirectory('%s') failed: %s", PathName, RtlGetStatusString(Status));
		OSFree(PathNameCopied);
		return Status;
	}
	
	// TODO: refactor or remove this whole override system, makes no sense.
	PPEB Peb = OSDLLGetCurrentPeb();

	HANDLE OldDirectory = Peb->CurrentDirectory.Handle;
	char* OldPath = Peb->CurrentDirectory.PathAllocated ? Peb->CurrentDirectory.Path : NULL;
	
	Peb->CurrentDirectory.Handle = NewDirectory;
	Peb->CurrentDirectory.Path = PathNameCopied;
	Peb->CurrentDirectory.PathAllocated = true;
	
	if (OldDirectory)
		OSClose(OldDirectory);
	
	if (OldPath)
		OSFree(OldPath);
	
	return STATUS_SUCCESS;
}

BSTATUS OSGetCurrentDirectoryPath(char* OutBuffer, size_t BufferSize)
{
	if (BufferSize == 0) {
		return STATUS_INVALID_PARAMETER;
	}
	
	PPEB Peb = OSDLLGetCurrentPeb();
	BSTATUS Status = STATUS_SUCCESS;
	
	// If the path is null or empty, copy an empty string.
	// Since the buffer size is at least one, this is OK to do.
	if (!Peb->CurrentDirectory.Path || !*Peb->CurrentDirectory.Path) {
		strcpy(OutBuffer, "");
		return STATUS_SUCCESS;
	}
	
	size_t PathLength = strlen(Peb->CurrentDirectory.Path);
	ASSERT(PathLength > 0 && "We already checked that earlier and it was good");
	
	// CopySize = how many bytes to copy, excluding null terminator.
	size_t CopySize = PathLength;
	if (CopySize > BufferSize - 1) {
		Status = STATUS_BUFFER_OVERFLOW;
		CopySize = BufferSize - 1;
	}
	
	memcpy(OutBuffer, Peb->CurrentDirectory.Path, CopySize);
	OutBuffer[CopySize] = 0;
	
	return Status;
}
