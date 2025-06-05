/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm4tst.c
	
Abstract:
	This module implements the MM5 test for the test driver.
	It tests the functionality of the view cache manager (Cc).
	
Author:
	iProgramInCpp - 5 June 2025
***/
#include <mm.h>
#include <io.h>
#include <ex.h>
#include <cc.h>
#include <string.h>
#include "utils.h"

static const char FileToOpen[] = "\\Devices\\Nvme0Disk1";
//static const char FileToOpen[] = "\\InitRoot\\test.sys";

void PerformMm5Test_()
{
	const int Offset = 0;
	const bool DoUncachedRead = true;
	
	BSTATUS Status;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ObjectAttributes.RootDirectory = HANDLE_NONE;
	ObjectAttributes.ObjectName = FileToOpen;
	ObjectAttributes.ObjectNameLength = sizeof(FileToOpen) - 1;
	
	LogMsg("Opening file %s", FileToOpen);
	
	Status = OSOpenFile(&FileHandle, &ObjectAttributes);
	if (FAILED(Status))
		KeCrash("Mm5: Cannot open file %s: %d", FileToOpen, Status);
	
	// ok now we need to reference the object from the handle
	PFILE_OBJECT FileObject = NULL;
	Status = ObReferenceObjectByHandle(FileHandle, IoFileType, (void**) &FileObject);
	if (FAILED(Status))
		KeCrash("Mm5: Cannot reference object by handle: %d", Status);
	
	size_t RegionSize = 32768;
	void* BaseAddress = MmAllocatePool(POOL_NONPAGED, RegionSize);
	void* BaseAddress2 = MmAllocatePool(POOL_NONPAGED, RegionSize);
	if (!BaseAddress || !BaseAddress2)
		KeCrash("Mm5: Could not allocate space for Base Address");
	
	if (DoUncachedRead)
	{
		IO_STATUS_BLOCK Iosb;
		Status = OSReadFile(&Iosb, FileHandle, Offset, BaseAddress2, RegionSize, 0);
		if (FAILED(Status))
			KeCrash("Mm5: Cannot perform uncached read from file %s: %d (%s)", FileToOpen, Status, RtlGetStatusString(Status));
	}
	
	// BaseAddress will be the Cc read.
	// BaseAddress2 will be the uncached read.
	Status = CcReadFileCopy(FileObject, Offset, BaseAddress, RegionSize);
	if (FAILED(Status))
		KeCrash("Mm5: Cannot perform cached read from file %s: %d (%s)", FileToOpen, Status, RtlGetStatusString(Status));
	
	// dump the first 512 bytes.
	LogMsg("Alright, read into %p, region size %zu.", BaseAddress, RegionSize);
	DumpHex(BaseAddress, 512, true);
	
	if (DoUncachedRead)
	{
		// Compare the two.
		if (memcmp(BaseAddress, BaseAddress2, RegionSize) != 0) {
			KeCrash("The comparison failed between the cached and uncached reads.");
		}
		else {
			LogMsg("The comparison succeeded, the cached and uncached reads returned identically.");
		}
	}
	else
	{
		LogMsg("Uncached read skipped.");
	}
	
	MmFreePool(BaseAddress);
	MmFreePool(BaseAddress2);
	
	LogMsg("Closing");
	Status = OSClose(FileHandle);
	if (FAILED(Status))
		KeCrash("Mm5: failed to close handle: %d", Status);
	
	ObDereferenceObject(FileObject);
}

void SimulateMemoryPressure();

void PerformMm5Test()
{
	LogMsg("Performing delay so everything calms down");
	//PerformDelay(5000, NULL);
	PerformDelay(1000, NULL);
	
	LogMsg("Performing Mm5 test once...");
	PerformMm5Test_();
	//LogMsg("Performing Mm5 test once more...");
	//PerformMm5Test_();
	//LogMsg("Performing Mm5 test one last time...");
	//PerformMm5Test_();
	//
	//SimulateMemoryPressure();
	//
	//LogMsg("Performing Mm5 test one last time...");
	//PerformMm5Test_();
}
