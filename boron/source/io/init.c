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
#include <string.h>

POBJECT_TYPE IopDriverType, IopDeviceType, IopFileType;

bool IopInitializeObjectTypes()
{
	BSTATUS Status;
	OBJECT_TYPE_INFO Info;
	memset (&Info, 0, sizeof Info);
	
	// Initialize common fields.
	Info.NonPagedPool = true;
	
	// Initialize the Driver object type.
	Info.Delete = IopDeleteDriver;
	Status = ObCreateObjectType("Driver", &Info, &IopDriverType);
	
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Driver type.");
		return false;
	}
	
	// Initialize the Device object type.
	Info.Delete = IopDeleteDevice;
	Status = ObCreateObjectType("Device", &Info, &IopDeviceType);
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Device type.");
		return false;
	}
	
	// Initialize the File object type.
	Info.MaintainHandleCount = true;
	Info.Delete = IopDeleteFile;
	Info.Close = IopCloseFile;
	Info.Parse = IopParseFile;
	Status = ObCreateObjectType("File", &Info, &IopFileType);
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create File type.");
		return false;
	}
	
	// All done.
	return true;
}

bool IoInitSystem()
{
	if (!IopInitializeDevicesDir())
		return false;
	
	if (!IopInitializeDriversDir())
		return false;
	
	if (!IopInitializeObjectTypes())
		return false;
	
	extern POBJECT_DIRECTORY ObpRootDirectory;
	extern BSTATUS ObpDebugDirectory(void* DirP);
	ObpDebugDirectory(ObpRootDirectory);
	
	// Initialize all drivers.
	LdrInitializeDrivers();
	
	return true;
}
