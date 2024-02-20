/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/devobj.h
	
Abstract:
	
	
Author:
	iProgramInCpp - 16 February 2024
***/
#pragma once

struct _DEVICE_OBJECT
{
	// Number of references.
	int RefCount;
	
	// The driver object that we belong to.
	PDRIVER_OBJECT DriverObject;
	
	// The entry in the driver object's list of devices.
	LIST_ENTRY ListEntry;
	
	// Extension data.
	size_t ExtensionSize;
	char Extension[];
};
