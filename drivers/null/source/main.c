/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function of the null
	driver.

Author:
	iProgramInCpp - 12 November 2026
***/
#include <ke.h>
#include <io.h>
#include <mm.h>
#include <string.h>

PDRIVER_OBJECT gDriverObject;
PDEVICE_OBJECT gDeviceObject;

BSTATUS NullRead(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	UNUSED PMDL Mdl,
	UNUSED uint32_t Flags
)
{
	Iosb->BytesRead = 0;
	Iosb->Status = STATUS_SUCCESS;
	return STATUS_SUCCESS;
}

BSTATUS NullWrite(
	PIO_STATUS_BLOCK Iosb,
	UNUSED PFCB Fcb,
	UNUSED uint64_t Offset,
	UNUSED PMDL Mdl,
	UNUSED uint32_t Flags
)
{
	Iosb->BytesWritten = Mdl->ByteCount;
	Iosb->Status = STATUS_SUCCESS;
	return STATUS_SUCCESS;
}

bool NullIsSeekable(UNUSED PFCB Fcb)
{
	return false;
}

IO_DISPATCH_TABLE NullDispatchTable =
{
	.Read = &NullRead,
	.Write = &NullWrite,
	.Seekable = &NullIsSeekable
};

BSTATUS DriverEntry(PDRIVER_OBJECT Object)
{
	BSTATUS Status;

	gDriverObject = Object;

	Status = IoCreateDevice(
		gDriverObject,
		0, // DeviceExtensionSize
		0, // FcbExtensionSize
		"Null",
		DEVICE_TYPE_CHARACTER,
		false,
		&NullDispatchTable,
		&gDeviceObject
	);

	if (FAILED(Status))
	{
		LogMsg("Null driver's IoCreateDevice call failed: %s", RtlGetStatusString(Status));
		return STATUS_UNLOAD;
	}

	return STATUS_SUCCESS;
}
