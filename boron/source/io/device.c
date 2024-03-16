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
POBJECT_TYPE IopDeviceType;
OBJECT_TYPE_INFO IopDeviceTypeInfo;

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

bool IopInitializeDeviceType()
{
	// TODO: Initialize IopDeviceTypeInfo
	
	BSTATUS Status = ObCreateObjectType(
		"Device",
		&IopDeviceTypeInfo,
		&IopDeviceType
	);
	
	if (FAILED(Status))
	{
		DbgPrint("IO: Failed to create Device type");
		return false;
	}
	
	return true;
}
