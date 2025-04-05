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
#include <ke.h>
#include <mm.h>
#include "utils.h"

static const char FileToOpen[] = "\\Device\\Nvme0Disk1";

void PerformMm3Test()
{
	BSTATUS Status;
	HANDLE FileHandle;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ObjectAttributes.RootDirectory = HANDLE_NONE;
	ObjectAttributes.ObjectName = FileToOpen;
	ObjectAttributes.ObjectNameLength = sizeof(FileToOpen) - 1;
	
	Status = OSOpenFile(&FileHandle, &ObjectAttributes);
	if (FAILED(Status))
		KeCrash("Mm3: Cannot open file %s: %d", FileToOpen, Status);
	
	
}
