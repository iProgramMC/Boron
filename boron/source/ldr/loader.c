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
#include <ke.h>

// TODO: Perhaps we could define it from the command line? Something like /HAL=<halfile>
#ifdef TARGET_AMD64

static uintptr_t LdrpCurrentBase = 0xFFFFF00000000000;
static const char* LdrpHalPathDefault = "halx86.sys";

#elif defined TARGET_I386

static uintptr_t LdrpCurrentBase = 0xD2000000;
static const char* LdrpHalPathDefault = "hali386.sys"; // sorry bucko, halx86 is already taken

#elif defined TARGET_ARM

static uintptr_t LdrpCurrentBase = 0xD2000000;
static const char* LdrpHalPathDefault = "hals5l8720.sys";

#else
	
#error Define your loader base and HAL default path here.

#endif

static const char* LdrpHalPath;

INIT
uintptr_t LdrAllocateRange(size_t Size)
{
	// note: Atomic operations aren't really needed in this case.
	// But I like my one liner
	return AtFetchAdd(LdrpCurrentBase, Size * PAGE_SIZE);
}

INIT
static bool LdriEndsWith(const char* String, const char* EndsWith)
{
	size_t Length = strlen(String);
	size_t EWLength = strlen(EndsWith);
	
	if (Length < EWLength)
		return false;
	
	return strcmp(String + Length - EWLength, EndsWith) == 0;
}

// Fix up the path - i.e. get just the file name, not the entire path.
INIT
static void LdriFixUpPath(PLOADER_MODULE File)
{
	size_t PathLength = strlen(File->Path);
	char* PathPtr = File->Path;
	
	for (size_t i = PathLength - 1; i < PathLength; i--)
	{
		if (File->Path[i] == '/')
		{
			PathPtr = &File->Path[i + 1];
			break;
		}
	}
	
	File->Path = PathPtr;
}

INIT
static void LdriLoadFile(PLOADER_MODULE File)
{
	if (LdriEndsWith(File->Path, ".sys"))
		LdriLoadDll(File);
}

static PLOADER_MODULE HalFile = NULL;

const char* LdrGetHalStringFromConfig()
{
	// have to scan the string manually, because ExInitBootConfig depends on
	// the memory manager and whatnot
	static char Buffer[64];
	
	bool Found = false;
	const char* CmdLine = KeGetBootCommandLine();
	while (*CmdLine)
	{
		if (memcmp(CmdLine, "Hal=", 4) != 0) {
			CmdLine++;
			continue;
		}
		
		// found the HAL string.
		CmdLine += 4;
		for (size_t i = 0; i < sizeof Buffer && CmdLine[i] != 0 && CmdLine[i] != ' '; i++) {
			Buffer[i] = CmdLine[i];
		}
		
		Found = true;
		break;
	}
	
	if (Found)
		return Buffer;
	
	return LdrpHalPathDefault;
}

const char* LdrGetHalName()
{
	return LdrpHalPath;
}

// Initializes the DLL loader and loads the boot modules.
INIT
void LdrInit()
{
	PLOADER_MODULE_INFO ModuleInfo = &KeLoaderParameterBlock.ModuleInfo;
	DbgPrint("Loaded Modules: %zu", ModuleInfo->Count);
	
	// note: we want to load the HAL first
	// so, get the name of the HAL from the command line.
	// If unspecified, use the default.
	LdrpHalPath = LdrGetHalStringFromConfig();
	DbgPrint("Using HAL '%s'.", LdrpHalPath);
	
	for (size_t i = 0; i < ModuleInfo->Count; i++)
	{
		PLOADER_MODULE File = &ModuleInfo->List[i];
		LdriFixUpPath(File);
		
		if (strcmp(File->Path, LdrpHalPath) == 0)
			HalFile = File;
	}
	
	if (!HalFile)
	{
		KeCrashBeforeSMPInit("No HAL loaded");
		return;
	}
	
	LdriLoadDll(HalFile);
}

INIT
static void LdrpReclaimFile(PLOADER_MODULE File)
{
#ifdef IS_64_BIT
	if ((uintptr_t)File->Address < (uintptr_t)MmGetHHDMBase() ||
		(uintptr_t)File->Address >= MM_PFNDB_BASE)
	{
		DbgPrint("Warning: file %s can't be reclaimed as it's not part of the HHDM", File->Path);
	}
	
	uintptr_t Address = (uintptr_t)File->Address;
	for (size_t j = 0; j < File->Size; j += PAGE_SIZE)
	{
		int Pfn = MmPhysPageToPFN(MmGetHHDMOffsetFromAddr((void*)Address));
		MmFreePhysicalPage(Pfn);
		Address += PAGE_SIZE;
	}
#else
	(void) File;
#endif
}

// NOTE: For now, selectively reclaim certain pages.  At some point, we'll reclaim everything, and scrap this function
INIT
static void LdrpReclaimKernelFile()
{
	LdrpReclaimFile(&KeLoaderParameterBlock.ModuleInfo.Kernel);
}

INIT
void LdrInitAfterHal()
{
	PLOADER_MODULE_INFO ModuleInfo = &KeLoaderParameterBlock.ModuleInfo;
	for (uint64_t i = 0; i < ModuleInfo->Count; i++)
	{
		PLOADER_MODULE File = &ModuleInfo->List[i];
		if (File == HalFile) continue;
		
		LdriLoadFile(File);
	}
	
	LdrpReclaimKernelFile();
}
