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
#include "iop.h"

bool IoInitSystem()
{
	if (!IopInitializeDevicesDir())
		return false;
	
	if (!IopInitializeDeviceType())
		return false;
	
	if (!IopInitializeDriverType())
		return false;
	
	extern POBJECT_DIRECTORY ObpRootDirectory;
	extern BSTATUS ObpDebugDirectory(void* DirP);
	ObpDebugDirectory(ObpRootDirectory);
	
	// Initialize all drivers.
	LdrInitializeDrivers();
	
	return true;
}
