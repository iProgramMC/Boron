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

INIT
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

BSTATUS IopCreateDeviceFileObject(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT* OutObject, uint32_t Flags, uint32_t OpenFlags)
{
	BSTATUS Status = IopCreateFileObject(DeviceObject->Fcb, OutObject, Flags, OpenFlags);
	
	// TODO: Permission inheritance
	
	return Status;
}

BSTATUS IoCreateDevice(
	PDRIVER_OBJECT DriverObject,
	size_t DeviceExtensionSize,
	size_t FcbExtensionSize,
	const char* DeviceName,
	DEVICE_TYPE Type,
	UNUSED bool Permanent,
	PIO_DISPATCH_TABLE DispatchTable,
	PDEVICE_OBJECT* OutDeviceObject
)
{
	PDEVICE_OBJECT DeviceObject = NULL;
	BSTATUS Status;
	
	Status = ObCreateObject(
		(void**) &DeviceObject,
		IoDevicesDir,
		IoDeviceType,
		DeviceName,
		OB_FLAG_PERMANENT,
		NULL,
		sizeof (DEVICE_OBJECT) + DeviceExtensionSize
	);
	// ^^^^ TODO: All devices are permanent for now.  Implement IopDeleteDevice
	//            and then come back and fix this.
	
	if (FAILED(Status))
		return Status;
	
	*OutDeviceObject = DeviceObject;
	
	// Initialize the device object.
	DeviceObject->DeviceType    = Type;
	DeviceObject->DriverObject  = DriverObject;
	DeviceObject->DispatchTable = DispatchTable;
	
	// Create the associated FCB.
	DeviceObject->Fcb = IoAllocateFcb(
		DeviceObject,
		FcbExtensionSize,
		true
	);
	
	if (!DeviceObject->Fcb)
	{
		DbgPrint("ERROR: Could not allocate FCB for device");
		ObUnlinkObject(DeviceObject);
		ObDereferenceObject(DeviceObject);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	// Insert the device into the driver's list of devices.
	KIPL Ipl;
	KeAcquireSpinLock(&DriverObject->DeviceListLock, &Ipl);
	InsertTailList(&DriverObject->DeviceList, &DeviceObject->ListEntry);
	KeReleaseSpinLock(&DriverObject->DeviceListLock, Ipl);
	
	return STATUS_SUCCESS;
}

void IopDeleteDevice(void* Object)
{
	// TODO
	// Steps I know so far:
	// - Remove the object from the driver object's device list
	// - Update IoCreateDevice to make the object not always permanent
	
	DbgPrint("UNIMPLEMENTED: IopDeleteDevice(%p)", Object);
}

// IopParseDevice is implemented in io/parse.c
