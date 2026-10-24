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
	// TODO: determine this automatically.
	return false;
}

NO_RETURN
void TestItNowThread(UNUSED void* Context)
{
	OSSleep(1000);

	BSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	OBJECT_ATTRIBUTES Attributes;
	HANDLE Handle;
	Attributes.RootDirectory = HANDLE_NONE;
	Attributes.OpenFlags = 0;
	Attributes.ObjectName = "/::/Devices/NorFlash";
	Attributes.ObjectNameLength = strlen(Attributes.ObjectName);

	Status = OSCreateFile(
		&Handle,
		&Attributes,
		&Iosb,
		0, // FileAttributes
		0, // AllocationSize
		IO_OPEN,
		IO_FILE_ANY_TYPE,
		NULL, // TargetName
		0     // TargetLength
	);

	if (FAILED(Status)) {
		LogMsg("NOR: failed to open %s: %s", Attributes.ObjectName, RtlGetStatusString(Status));
		OSExitThread();
	}

	uint64_t Offset = 0x8400;

	uint32_t Buffer[4];
	Status = OSReadFile(
		&Iosb,
		Handle,
		Offset,
		Buffer,
		sizeof Buffer,
		0  // Flags
	);

	if (FAILED(Status))
	{
		LogMsg("zephyr2: Could not read %zu bytes from %s: %s", sizeof(Buffer), Attributes.ObjectName, RtlGetStatusString(Status));
		OSExitThread();
	}

	LogMsg("Hello from nor driver!");
	LogMsg("16 bytes at offset %llu: %08x %08x %08x %08x", Offset, Buffer[0], Buffer[1], Buffer[2], Buffer[3]);

	OSClose(Handle);
	OSExitThread();
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

	PETHREAD Thread;
	Status = PsCreateSystemThreadFast(
		&Thread,
		TestItNowThread,
		NULL, // StartContext
		false // CreateSuspended
	);

	if (FAILED(Status)) {
		LogMsg("NOR: Cannot create test thread: %s", RtlGetStatusString(Status));
	}
	else {
		ObDereferenceObject(Thread);
	}

	return STATUS_SUCCESS;
}
