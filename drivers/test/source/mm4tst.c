/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm4tst.c
	
Abstract:
	This module implements the MM4 test for the test driver.
	It tests the functionality of MmMapViewOfFileInSystemSpace
	and related functions.
	
Author:
	iProgramInCpp - 29 May 2025
***/
#include <mm.h>
#include <io.h>
#include <ex.h>
#include <string.h>
#include "utils.h"

static const char FileToOpen[] = "\\Devices\\Nvme0Disk1";
//static const char FileToOpen[] = "\\InitRoot\\test.sys";

void PerformMm4Test_()
{
	const int Offset = 0; // was 20
	
	BSTATUS Status;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ObjectAttributes.RootDirectory = HANDLE_NONE;
	ObjectAttributes.ObjectName = FileToOpen;
	ObjectAttributes.ObjectNameLength = sizeof(FileToOpen) - 1;
	
	LogMsg("Opening file %s", FileToOpen);
	
	Status = OSOpenFile(&FileHandle, &ObjectAttributes);
	if (FAILED(Status))
		KeCrash("Mm4: Cannot open file %s: %d", FileToOpen, Status);
	
	// ok now we need to reference the object from the handle
	PFILE_OBJECT FileObject = NULL;
	Status = ObReferenceObjectByHandle(FileHandle, IoFileType, (void**) &FileObject);
	if (FAILED(Status))
		KeCrash("Mm4: Cannot reference object by handle: %d", Status);
	
	LogMsg("Mapping it");
	void* BaseAddress = NULL;
	size_t RegionSize = 32768;
	Status = MmMapViewOfFileInSystemSpace(FileObject, &BaseAddress, RegionSize, 0, Offset, PAGE_READ);
	if (FAILED(Status))
		KeCrash("Mm4: Cannot map file: %d", FileToOpen, Status);
	
	// dump the first 512 bytes.
	LogMsg("Alright, mapped at %p, region size %zu.", BaseAddress, RegionSize);
	DumpHex(BaseAddress, 512, true);
	
	// now unmap the view
	LogMsg("Freeing");
	MmUnmapViewOfFileInSystemSpace(BaseAddress, true);
	
	LogMsg("Closing");
	Status = OSClose(FileHandle);
	if (FAILED(Status))
		KeCrash("Mm4: failed to close handle: %d", Status);
	
	ObDereferenceObject(FileObject);
}

void SimulateMemoryPressure();

void PerformMm4Test()
{
	LogMsg("Performing delay so everything calms down");
	//PerformDelay(5000, NULL);
	PerformDelay(1000, NULL);
	
	LogMsg("Performing Mm4 test once...");
	PerformMm4Test_();
	LogMsg("Performing Mm4 test once more...");
	PerformMm4Test_();
	LogMsg("Performing Mm4 test one last time...");
	PerformMm4Test_();
	
	SimulateMemoryPressure();
	
	LogMsg("Performing Mm4 test one last time...");
	PerformMm4Test_();
}
