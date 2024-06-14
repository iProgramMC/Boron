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

bool IopInitializeDevicesDir()
{
	BSTATUS Status = ObCreateDirectoryObject(
		&IoDevicesDir,
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

BSTATUS IopParseDevice(void* Object, UNUSED const char** Name, UNUSED void* Context, UNUSED int LoopCount, UNUSED void** OutObject)
{
	// TODO
	DbgPrint("UNIMPLEMENTED: IopParseFile(%p)", Object);
	return STATUS_UNIMPLEMENTED;
}
