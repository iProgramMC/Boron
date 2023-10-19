/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          loader.c                           *
\*************************************************************/
#include "ldrint.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"

#define TAG(str) *((int*)str)

typedef struct limine_file LIMINE_FILE, *PLIMINE_FILE;

enum
{
	EXTENSION_UNK,
	EXTENSION_TXT,
	EXTENSION_EXE,
	EXTENSION_DLL,
	EXTENSION_SYS,
};

static bool EndsWith(const char* String, const char* EndsWith)
{
	size_t Length = strlen(String);
	size_t EWLength = strlen(EndsWith);
	
	if (Length < EWLength)
		return false;
	
	return strcmp(String + Length - EWLength, EndsWith) == 0;
}

// Fix up the path - i.e. get just the file name, not the entire path.
// ExtensionOut - the TAG()'d version of the zero-padded extension, or the last 4 characters of the extension
// ex. "x.so" -> 'os',  "x.dll" -> 'lld',  "x.stuff" -> 'ffut'
void FixUpPath(PLIMINE_FILE File)
{
	size_t PathLength = strlen(File->path);
	char* PathPtr = File->path;
	
	while (PathLength--)
	{
		if (*PathPtr == '/')
		{
			PathPtr++;
			break;
		}
		
		PathPtr--;
	}
	
	File->path = PathPtr;
	PathLength = strlen(File->path);
}

void UseSharedModuleFile(PLIMINE_FILE File)
{
	// TODO
	LogMsg("UseSharedModuleFile(%s) UNIMPLEMENTED", File->path);
}

void UseTextFile(PLIMINE_FILE File)
{
	
}

void UseFile(PLIMINE_FILE File)
{
	LogMsg("loading: %s", File->path);
	
	if (EndsWith(File->path, ".txt"))
	{
		char Buffer[32];
		sprintf(Buffer, "Text file contents: %%.%zus", File->size);
		LogMsg(Buffer, File->address);
		return;
	}
	
	LogMsg("TODO: Load Module");
}

void LoadModules()
{
	struct limine_module_response* Response = ModuleRequest.response;
	if (!Response)
	{
		LogMsg("Got no response to the module request");
	}
	else
	{
		LogMsg("Loaded Modules: %d", Response->module_count);
		
		for (uint64_t i = 0; i < Response->module_count; i++)
		{
			PLIMINE_FILE File = Response->modules[i];
			FixUpPath(File);
			UseFile(File);
		}
	}
}
