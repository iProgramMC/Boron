/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/drvobj.h
	
Abstract:
	This header file defines the I/O driver object structure.
	
Author:
	iProgramInCpp - 16 February 2024
***/
#pragma once

#include "dispatch.h"

typedef BSTATUS(*PDRIVER_ENTRY)(PDRIVER_OBJECT Object);
typedef BSTATUS(*PDRIVER_UNLOAD)(PDRIVER_OBJECT Object);
typedef BSTATUS(*PDRIVER_ADD_DEVICE)(PDRIVER_OBJECT Object, PDEVICE_OBJECT Device);

struct _DRIVER_OBJECT
{
	// Flags.
	uint32_t Flags;
	uint32_t Available;
	
	// Name of the driver.  Assigned by the driver loader.
	const char* DriverName;
	
	// Pointer to extended data stored by the driver. The kernel does
	// not use this pointer - it's entirely driver-controlled.
	void* DriverExtension;
	
	// List of device objects implemented by this driver.
	LIST_ENTRY DeviceList;
	
	// The entry point of the driver.
	PDRIVER_ENTRY DriverEntry;
	
	// The routine called when the driver is to be unloaded. If can't
	// be unloaded, this is NULL.
	PDRIVER_UNLOAD Unload;
	
	// The routine called when a device is added.  This is called in
	// the case that the driver passes off the request to look for
	// specific devices to the kernel. For example, a driver that
	// operates devices on the PCI bus with a specific vendor and
	// device ID can simply tell the kernel to lookup the specific
	// devices and add them.
	PDRIVER_ADD_DEVICE AddDevice;
};
