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

POBJECT_TYPE IoDriverType, IoDeviceType, IoFileType, IoControllerType;
POBJECT_DIRECTORY IoDriversDir;
POBJECT_DIRECTORY IoDevicesDir;

void* IoGetBuiltInData(int Number)
{
	switch (Number)
	{
		case __IO_DRIVER_TYPE: return IoDriverType;
		case __IO_DEVICE_TYPE: return IoDeviceType;
		case __IO_FILE_TYPE:   return IoFileType;
		case __IO_DRIVERS_DIR: return IoDriversDir;
		case __IO_DEVICES_DIR: return IoDevicesDir;
		case __IO_CNTRLR_TYPE: return IoControllerType;
		default:               return NULL;
	}
}

INIT
bool IopInitializeObjectTypes()
{
	BSTATUS Status;
	OBJECT_TYPE_INFO Info;
	memset (&Info, 0, sizeof Info);
	
	// Initialize common fields.
	Info.NonPagedPool = true;
	
	// Initialize the Driver object type.
	Info.Delete = IopDeleteDriver;
	Status = ObCreateObjectType("Driver", &Info, &IoDriverType);
	
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Driver type.");
		return false;
	}
	
	// Initialize the Device object type.
	Info.Delete = IopDeleteDevice;
	Info.Parse = IopParseDevice;
	Status = ObCreateObjectType("Device", &Info, &IoDeviceType);
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Device type.");
		return false;
	}
	
	// Initialize the Controller object type.
	Info.Delete = IopDeleteController;
	Info.Parse = NULL;
	Status = ObCreateObjectType("Controller", &Info, &IoControllerType);
	
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Controller type.");
		return false;
	}
	
	// Initialize the File object type.
	Info.MaintainHandleCount = true;
	Info.Open = IopOpenFile;
	Info.Close = IopCloseFile;
	Info.Parse = IopParseFile;
	Info.Delete = IopDeleteFile;
	Status = ObCreateObjectType("File", &Info, &IoFileType);
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create File type.");
		return false;
	}
	
	// All done.
	return true;
}

INIT
bool IoInitSystem()
{
	if (!IopInitializeDevicesDir())
		return false;
	
	if (!IopInitializeDriversDir())
		return false;
	
	if (!IopInitializeObjectTypes())
		return false;
	
	// TEST: This is test code. Remove soon!
#ifdef DEBUG
	extern POBJECT_DIRECTORY ObpRootDirectory;
	extern BSTATUS ObpDebugDirectory(void* DirP);
	DbgPrint("Dumping root directory:");
	ObpDebugDirectory(ObpRootDirectory);
#endif
	
	IopInitPartitionManager();
	
	LdrInitializeDrivers();
	
	IoScanForFileSystems();
	
	// TEST: This is test code. Remove soon!
#ifdef DEBUG
	extern POBJECT_DIRECTORY ObpObjectTypesDirectory;
	
	DbgPrint("Dumping drivers directory:");
	ObpDebugDirectory(IoDriversDir);
	DbgPrint("Dumping devices directory:");
	ObpDebugDirectory(IoDevicesDir);
	DbgPrint("Dumping types directory:");
	ObpDebugDirectory(ObpObjectTypesDirectory);
#endif
	
	return true;
}
