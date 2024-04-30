/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ldr/loader.c
	
Abstract:
	This module implements the driver DLL loader.
	
Author:
	iProgramInCpp - 22 October 2023
***/
#include "ldri.h"

static uintptr_t LdrpCurrentBase = 0xFFFFF00000000000;

// TODO: Perhaps we could define it from the command line? Something like /HAL=<halfile>
#ifdef TARGET_AMD64
static const char* LdrpHalPath = "halx86.sys";
#else
#error Define your HAL path here.
#endif

uintptr_t LdrAllocateRange(size_t Size)
{
	// note: Atomic operations aren't really needed in this case.
	// But I like my one liner
	return AtFetchAdd(LdrpCurrentBase, Size * PAGE_SIZE);
}

static bool LdriEndsWith(const char* String, const char* EndsWith)
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
static void LdriFixUpPath(PLIMINE_FILE File)
{
	size_t PathLength = strlen(File->path);
	char* PathPtr = File->path;
	
	for (size_t i = PathLength - 1; i < PathLength; i--)
	{
		if (File->path[i] == '/')
		{
			PathPtr = &File->path[i + 1];
			break;
		}
	}
	
	File->path = PathPtr;
}

static void LdriLoadFile(PLIMINE_FILE File)
{
	if (LdriEndsWith(File->path, ".sys") || LdriEndsWith(File->path, ".dll"))
	{
		LdriLoadDll(File);
	}
	else
	{
		DbgPrint("Unknown file extension for file %s", File->path);
	}
}

// Initializes the DLL loader and loads the boot modules.
void LdrInit()
{
	struct limine_module_response* Response = KeLimineModuleRequest.response;
	if (!Response)
	{
		KeCrashBeforeSMPInit("No response to the module request from limine?!");
		return;
	}

	DbgPrint("Loaded Modules: %d", Response->module_count);
	
	// note: we want to load the HAL first
	PLIMINE_FILE HalFile = NULL;
	
	for (uint64_t i = 0; i < Response->module_count; i++)
	{
		PLIMINE_FILE File = Response->modules[i];
		LdriFixUpPath(File);
		
		if (strcmp(File->path, LdrpHalPath) == 0)
			HalFile = File;
	}
	
	if (!HalFile)
	{
		KeCrashBeforeSMPInit("No HAL loaded");
		return;
	}
	
	LdriLoadFile(HalFile);
	
	for (uint64_t i = 0; i < Response->module_count; i++)
	{
		PLIMINE_FILE File = Response->modules[i];
		if (File == HalFile) continue;
		
		LdriLoadFile(File);
	}
}
