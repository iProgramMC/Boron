/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function for the
	iPod/iPhone NOR flash driver.

Author:
	iProgramInCpp - 24 October 2026
***/
#include "nor.h"
#include <string.h>
#include <ps.h>

PDRIVER_OBJECT gDriverObject;
PDEVICE_OBJECT gDeviceObject;

bool UseSPIAccess()
{
	const char* Model = KeLoaderParameterBlock.HardwareIdentifier;

	return strcmp(Model, "m68ap") != 0 && // iPhone 2G
	       strcmp(Model, "n45ap") != 0;   // iPod touch 1G
}

size_t NorSize()
{
	return 16 * 1024 * 1024;
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	gDriverObject = DriverObject;

	PIO_DISPATCH_TABLE DispatchTable;

	bool Result = false;
	if (UseSPIAccess()) {
		DispatchTable = &NorSpiDispatchTable;
		Result = NorSpiInit();
	}
	else {
		DispatchTable = &NorCfiDispatchTable;
		Result = NorCfiInit();
	}

	if (!Result)
	{
		LogMsg("NOR initialization failed.");
		return STATUS_UNLOAD;
	}

	BSTATUS Status = IoCreateDevice(
		gDriverObject,
		0,  // DeviceExtensionSize
		0,  // FcbExtensionSize
		"NorFlash",
		DEVICE_TYPE_BLOCK,
		true, // Permanent
		DispatchTable,
		&gDeviceObject
	);

	if (FAILED(Status))
	{
		LogMsg("NOR: Failed to create device object: %s", RtlGetStatusString(Status));
		return STATUS_UNLOAD;
	}

	// Assign it a size manually.
	gDeviceObject->Fcb->FileLength = NorSize();

	return STATUS_SUCCESS;
}
