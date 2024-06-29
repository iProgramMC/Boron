/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/devobj.h
	
Abstract:
	This header file defines the I/O device object structure.
	
Author:
	iProgramInCpp - 16 February 2024
***/
#pragma once

#include "dispatch.h"

struct _DEVICE_OBJECT
{
	// Number of references.
	int RefCount;
	
	// The driver object that we belong to.
	PDRIVER_OBJECT DriverObject;
	
	// The entry in the driver object's list of devices.
	LIST_ENTRY ListEntry;
	
	// A pointer to the driver object's dispatch table.
	PIO_DISPATCH_TABLE DispatchTable;
	
	// Extension data.
	size_t ExtensionSize;
	char Extension[];
};

//BSTATUS IoCreate
