/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/device.c
	
Abstract:
	This module implements the I/O Device object type.
	
Author:
	iProgramInCpp - 26 February 2024
***/
#include "iop.h"

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
	{
		DbgPrint("IO: Failed to create \\Devices directory");
		return false;
	}
	
	// The devices directory is now created.
	// N.B.  We keep a permanent reference to it at all times, will be useful.
	
	return true;
}

void IopDeleteDevice(void* Object)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopDeleteDevice(%p)", Object);
}
