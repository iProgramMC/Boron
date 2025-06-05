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

// Supported types of devices:
typedef enum _DEVICE_TYPE
{
	DEVICE_TYPE_UNKNOWN,
	DEVICE_TYPE_CHARACTER, // Terminals, keyboards, mice, etc.
	DEVICE_TYPE_BLOCK,     // Disk drives, CD-ROM drives, etc.
	DEVICE_TYPE_VIDEO,     // Frame buffer devices
	DEVICE_TYPE_NETWORK,   // Network adapter devices
}
DEVICE_TYPE;

typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;

struct _DEVICE_OBJECT
{
	// The type of device object this is.
	DEVICE_TYPE DeviceType;
	
	// The driver object that we belong to.
	PDRIVER_OBJECT DriverObject;
	
	// The entry in the driver object's list of devices.
	LIST_ENTRY ListEntry;
	
	// A pointer to the I/O operation dispatch table.
	PIO_DISPATCH_TABLE DispatchTable;
	
	PFCB Fcb;
	
	// If this is part of a controller, this is the pointer to that controller.
	PCONTROLLER_OBJECT ParentController;
	
	// If this is part of a controller, this is the entry into the controller's
	// device tree.
	RBTREE_ENTRY DeviceTreeEntry;
	
	// If this is a partition, this is the device object this one belongs to
	// (Note: This object is referenced for the lifetime of this device)
	PDEVICE_OBJECT ParentDevice;
	
	// If this is a mounted partition, this is the file object pointing to the
	// root of said mount.
	// TODO: Maybe this belongs in a separate object?
	PFILE_OBJECT MountRoot;
	
	// If this is a partitionable device, this is an entry into the list of
	// partitionable objects.
	LIST_ENTRY PartitionableListEntry;
	
	// Extension data.
	size_t ExtensionSize;
	char Extension[];
};

BSTATUS IoCreateDevice(
	PDRIVER_OBJECT DriverObject,
	size_t DeviceExtensionSize,
	size_t FcbExtensionSize,
	const char* DeviceName,
	DEVICE_TYPE Type,
	bool Permanent,
	PIO_DISPATCH_TABLE DispatchTable,
	PDEVICE_OBJECT* OutDeviceObject
);

BSTATUS IoReadDevice(
	PIO_STATUS_BLOCK Iosb,
	PDEVICE_OBJECT DeviceObject,
	void* Buffer,
	size_t Size,
	uint64_t FileOffset,
	bool Cached
);

BSTATUS IoWriteDevice(
	PIO_STATUS_BLOCK Iosb,
	PDEVICE_OBJECT DeviceObject,
	const void* Buffer,
	size_t Size,
	uint64_t FileOffset,
	bool Cached
);

BSTATUS IoOpenDeviceObject(
	PDEVICE_OBJECT DeviceObject,
	PFILE_OBJECT* OutObject,
	uint32_t Flags,
	uint32_t OpenFlags
);
