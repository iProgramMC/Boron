#include <boron.h>
#include <string.h>
#include <rtl/cmdline.h>
#include "pebteb.h"
#include "dll.h"

typedef struct
{
	bool Initted;
	char* Continuation;
}
TOKENIZE_STATE, *PTOKENIZE_STATE;

#define CLEAR_TOKENIZE_STATE(ts) do { \
	(ts)->Initted = false; \
	(ts)->Continuation = NULL; \
} while (0)

// Adapted from https://github.com/iProgramMC/BetterStrTok
char* OSTokenizeString(PTOKENIZE_STATE State, char* String, const char* Separators)
{
	if (!State->Initted)
	{
		State->Initted = true;
		State->Continuation = NULL;
	}
	else if (!State->Continuation)
	{
		// No more continuation
		return NULL;
	}
	
	if (!String)
		String = State->Continuation;
	
	if (!String)
		return NULL;
	
	size_t Length = strlen(String);
	for (size_t i = 0; i < Length; i++)
	{
		if (strchr(Separators, String[i]) != NULL)
		{
			State->Continuation = &String[i + 1];
			String[i] = 0;
			return String;
		}
	}
	
	// Not found, hit null terminator.
	State->Continuation = NULL;
	return String;
}

const char* OSDLLGetEnvironmentVariable(const char* Key)
{
	char Buffer[128];
	
	if (strlen(Key) >= 128)
		return NULL;
	
	snprintf(Buffer, sizeof Buffer, "%s=", Key);
	size_t Length = strlen(Buffer);
	
	const char* Environment = OSDLLGetCurrentPeb()->Environment;
	
	// The environment string is a list of key=value blobs separated by null characters
	// and terminated by two null characters in a row.
	while (*Environment)
	{
		if (strlen(Environment) >= Length &&
			memcmp(Environment, Buffer, Length) == 0)
			return Environment + Length;
		
		Environment += strlen(Environment) + 1;
	}
	
	return NULL;
}

// Sets the current environment description.
//
// Notes:
// - The environment description is entirely owned by the calling thread.
//   This means that the calling thread is responsible for freeing the old
//   description if they wish to update it again.
//
// - This is thread unsafe against OSDLLGetEnvironmentVariable. So avoid
//   using the OSDLLGetEnvironmentVariable API in other threads while
//   modifying the environment description.
void OSDLLSetEnvironmentDescription(char* Description)
{
	PPEB Peb = OSDLLGetCurrentPeb();
	
	Peb->EnvironmentSize = RtlEnvironmentLength(Description);
	Peb->Environment = Description;
}

// Obtains a pointer to the current environment description.
//
// Note that the initial environment description is part of the PEB, so it
// cannot be freed like usual.  Do not free the first environment description.
const char* OSDLLGetEnvironmentDescription()
{
	return OSDLLGetCurrentPeb()->Environment;
}

HIDDEN
BSTATUS OSDLLOpenFileByName(PHANDLE Handle, const char* FileName, bool IsLibrary)
{
	BSTATUS Status;
	OBJECT_ATTRIBUTES Attributes;
	
	// Try the current directory first.
	// TODO: For known system shared libraries, do this last.  Or not at all.
	Attributes.ObjectName = FileName;
	Attributes.ObjectNameLength = strlen(FileName);
	Attributes.OpenFlags = 0;
	Attributes.RootDirectory = OSDLLGetCurrentDirectory();
	
	Status = OSOpenFile(Handle, &Attributes);
	if (SUCCEEDED(Status))
		return Status;
	
	if (*FileName == '/')
	{
		// Absolute path, clearly we can't find it looking through PATH.
		return STATUS_NAME_NOT_FOUND;
	}
	
	LdrDbgPrint("OSDLL: OSDLLOpenFileByName cannot open %s from the current directory.  Trying to scan PATH or LIB_PATH.", FileName);
	
	const char* Key = IsLibrary ? "LIB_PATH" : "PATH";
	const char* Path = OSDLLGetEnvironmentVariable(Key);
	if (!Path)
	{
		DbgPrint("OSDLL: The '%s' environment variable wasn't specified.", Key);
		return STATUS_NAME_NOT_FOUND;
	}
	
	size_t PathLength = strlen(Path);
	char* NewPath = OSAllocate(PathLength + 1);
	if (!NewPath)
		return STATUS_INSUFFICIENT_MEMORY;
	
	strcpy(NewPath, Path);
	
	TOKENIZE_STATE Tokenize;
	CLEAR_TOKENIZE_STATE(&Tokenize);
	
	char* Segment = OSTokenizeString(&Tokenize, NewPath, ":");
	while (Segment)
	{
		char ImageName[IO_MAX_NAME];
		snprintf(ImageName, sizeof ImageName, "%s/%s", Segment, FileName);
		LdrDbgPrint("OSDLL: Scanning %s, looking for %s.", Key, ImageName);
		
		Attributes.ObjectName = ImageName;
		Attributes.ObjectNameLength = strlen(ImageName);
		Attributes.OpenFlags = 0;
		Attributes.RootDirectory = HANDLE_NONE;
		
		Status = OSOpenFile(Handle, &Attributes);
		if (SUCCEEDED(Status))
			return Status;
		
		Segment = OSTokenizeString(&Tokenize, NULL, ":");
	}
	
	return STATUS_NAME_NOT_FOUND;
}
