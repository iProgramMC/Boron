/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function for the
	iPod/iPhone hardware configuration module.

Author:
	iProgramInCpp - 25 October 2026
***/
#include "syscfg.h"

IO_DISPATCH_TABLE DispatchTable =
{
	.Reference = SyscfgReferenceFcb,
	.Dereference = SyscfgDereferenceFcb,
	.Read = SyscfgRead,
	.ReadDir = SyscfgReadDir,
	.ParseDir = SyscfgParseDir,
	.Seekable = SyscfgSeekableFcb,
	.Flags = DISPATCH_FLAG_DEVICE_IS_DIRECTORY
};

PDRIVER_OBJECT gDriverObject;
PDEVICE_OBJECT gDeviceObject;

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	gDriverObject = DriverObject;

	BSTATUS Status = IoCreateDevice(
		gDriverObject,
		sizeof(DEVICE_EXT),
		sizeof(FCB_EXT),
		"HardwareConfiguration",
		DEVICE_TYPE_CHARACTER,
		true, // Permanent
		&DispatchTable,
		&gDeviceObject
	);

	if (FAILED(Status))
	{
		LogMsg("Syscfg: Failed to initialize: %s", RtlGetStatusString(Status));
		return STATUS_UNLOAD;
	}

	DbgPrint("My DO is %p", gDeviceObject);

	PDEVICE_EXT Ext = DEXT(gDeviceObject);
	ExInitializeFastMutex(&Ext->Mutex);
	InitializeListHead(&Ext->FcbList);

	return STATUS_SUCCESS;
}
