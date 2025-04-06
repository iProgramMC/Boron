/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm3tst.c
	
Abstract:
	This module implements the MM3 test for the test driver.
	It tests the functionality of MmMapViewOfObject and related
	functions.
	
Author:
	iProgramInCpp - 5 April 2025
***/
#include <mm.h>
#include <io.h>
#include <ex.h>
#include "utils.h"

static const char FileToOpen[] = "\\Devices\\Nvme0Disk1";

void PerformMm3Test_()
{
	BSTATUS Status;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ObjectAttributes.RootDirectory = HANDLE_NONE;
	ObjectAttributes.ObjectName = FileToOpen;
	ObjectAttributes.ObjectNameLength = sizeof(FileToOpen) - 1;
	
	LogMsg("Opening file %s", FileToOpen);
	
	Status = OSOpenFile(&FileHandle, &ObjectAttributes);
	if (FAILED(Status))
		KeCrash("Mm3: Cannot open file %s: %d", FileToOpen, Status);
	
	LogMsg("Mapping it");
	void* BaseAddress = NULL;
	size_t RegionSize = 32768;
	Status = MmMapViewOfObject(FileHandle, &BaseAddress, RegionSize, MEM_TOP_DOWN, 20, PAGE_READ);
	if (FAILED(Status))
		KeCrash("Mm3: Cannot map file: %d", FileToOpen, Status);
	
	// dump the first 512 bytes.
	LogMsg("Alright, mapped at %p, region size %zu.", BaseAddress, RegionSize);
	DumpHex(BaseAddress, 512, true);
	
	// now unmap the view
	LogMsg("Freeing");
	Status = OSFreeVirtualMemory(BaseAddress, RegionSize, MEM_RELEASE);
	if (FAILED(Status))
		KeCrash("Mm3: failed to free VM: %d", Status);
	
	LogMsg("Closing");
	Status = OSClose(FileHandle);
	if (FAILED(Status))
		KeCrash("Mm3: failed to close handle: %d", Status);
}


void PerformMm3Test()
{
	LogMsg("Performing Mm3 test once...");
	PerformMm3Test_();
	LogMsg("Performing Mm3 test once more...");
	PerformMm3Test_();
	LogMsg("Performing Mm3 test one last time...");
	PerformMm3Test_();
}
