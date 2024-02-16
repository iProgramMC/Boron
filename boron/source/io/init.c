/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/init.c
	
Abstract:
	This module implements the initialization procedure
	for the I/O subsystem.
	
Author:
	iProgramInCpp - 15 February 2024
***/
#include <io.h>

POBJECT_DIRECTORY IopDevicesDirectory;

bool IopInitializeDevicesDir()
{
	BSTATUS Status = ObCreateDirectoryObject(
		&IopDevicesDirectory,
		NULL,
		"\\Devices",
		OB_FLAG_KERNEL | OB_FLAG_PERMANENT
	);
	
	if (FAILED(Status))
		return false;
	
	// The devices directory is now created.
	// N.B.  Keep a permanent reference to it at all times, will be useful.
	
	return true;
}

bool IoInitSystem()
{
	if (!IopInitializeDevicesDir())
		return false;
	
	extern POBJECT_DIRECTORY ObpRootDirectory;
	extern BSTATUS ObpDebugDirectory(void* DirP);
	ObpDebugDirectory(ObpRootDirectory);
	
	return true;
}
