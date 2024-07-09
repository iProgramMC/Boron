/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	io/cntrobj.h
	
Abstract:
	This header file defines the I/O controller object structure.
	
Author:
	iProgramInCpp - 9 July 2024
***/
#pragma once

#include <rtl/avltree.h>

typedef struct _CONTROLLER_OBJECT
{
	PDRIVER_OBJECT DriverObject;
	
	KMUTEX DeviceTreeMutex;
	
	// Keyed by a driver specific uintptr-sized key. For example,
	// the slot number occupied by a device can be used as a key.
	AVLTREE DeviceTree;
	
	char Extension[];
}
CONTROLLER_OBJECT, *PCONTROLLER_OBJECT;

BSTATUS IoCreateController(
	PDRIVER_OBJECT DriverObject,
	size_t ControllerExtensionSize,
	const char* ControllerName,
	bool Permanent,
	PCONTROLLER_OBJECT* OutControllerObject
);

PDEVICE_OBJECT IoGetDeviceController(
	PCONTROLLER_OBJECT Controller,
	uintptr_t Key
);

// NOTE: Does not add 1 to the reference count of either the device or the
// controller.  After the call there can be two deletion scenarios:
//
// 1. The device is deleted.  It will then be removed from the controller
//    by the device's delete handler.  This is probably a driver-initiated
//    deletion anyway.
//
// 2. The controller is deleted.  For each device still connected, its
//    controller links are cleared.  The devices will be deleted when their
//    final reference is deleted.
BSTATUS IoAddDeviceController(
	PCONTROLLER_OBJECT Controller,
	uintptr_t Key,
	PDEVICE_OBJECT Device
);

BSTATUS IoRemoveDeviceController(
	PCONTROLLER_OBJECT Controller,
	PDEVICE_OBJECT Device
);
