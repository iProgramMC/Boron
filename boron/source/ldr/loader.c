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

// TODO: Perhaps we could define it from the command line? Something like /HAL=<halfile>
#ifdef TARGET_AMD64

static uintptr_t LdrpCurrentBase = 0xFFFFF00000000000;
static const char* LdrpHalPath = "halx86.sys";

#elif defined TARGET_I386

static uintptr_t LdrpCurrentBase = 0xD2000000;
static const char* LdrpHalPath = "hali386.sys"; // sorry bucko, halx86 is already taken

#elif defined TARGET_ARM

static uintptr_t LdrpCurrentBase = 0xD2000000;
static const char* LdrpHalPath = "halarmqv.sys"; // TODO: make this configurable. for now, ARM QEMU virt machine.

#else
	
#error Define your loader base and HAL path here.

#endif

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

// Initializes the DLL loader and loads the boot modules.
INIT
void LdrInit()
{
	PLOADER_MODULE_INFO ModuleInfo = &KeLoaderParameterBlock.ModuleInfo;
	DbgPrint("Loaded Modules: %zu", ModuleInfo->Count);
	
	// note: we want to load the HAL first
	for (size_t i = 0; i < ModuleInfo->Count; i++)
	{
		PLOADER_MODULE File = &ModuleInfo->List[i];
		LdriFixUpPath(File);
		
		if (strcmp(File->Path, LdrpHalPath) == 0)
			HalFile = File;
	}
	
	if (!HalFile)
	{
		/*
		KeCrashBeforeSMPInit("No HAL loaded");
		return;
		*/
		
		extern BSTATUS HAL_DriverEntry(PDRIVER_OBJECT Object);
		DbgPrint("calling fake built-in HAL");
		HAL_DriverEntry(NULL);
		return;
	}
	
	LdriLoadFile(HalFile);
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
