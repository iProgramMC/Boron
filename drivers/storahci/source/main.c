/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function for the
	AHCI device driver.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#include "ahci.h"
#include <string.h>

PDRIVER_OBJECT AhciDriverObject;

IO_DISPATCH_TABLE AhciDispatchTable;

// TODO: System wide solution.
int DriveNumber;

bool AhciPciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext)
{
	char Buffer[16];
	int Number = AtFetchAdd(DriveNumber, 1);
	snprintf (Buffer, sizeof Buffer, "AhciDisk%d", Number);
	
	PDEVICE_OBJECT DeviceObject;
	
	BSTATUS Status = IoCreateDevice(
		AhciDriverObject,
		sizeof(DEVICE_EXTENSION),
		sizeof(FCB_EXTENSION),
		Buffer,
		DEVICE_TYPE_BLOCK,
		true, // TODO: false
		&AhciDispatchTable,
		&DeviceObject
	);
	
	if (FAILED(Status))
	{
		LogMsg("Ahci: Failed to create AHCI disk %d. Status: %d", Number, Status);
		return false;
	}
	
	return true;
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	AhciDriverObject = DriverObject;
	
	// TODO:
	//AhciDispatchTable
	
	BSTATUS Status = HalPciEnumerate(
		false,
		0,
		NULL,
		PCI_CLASS_MASS_STORAGE,
		PCI_SUBCLASS_SATA,
		AhciPciDeviceEnumerated,
		NULL
	);
	
	if (Status == STATUS_NO_SUCH_DEVICES)
		Status =  STATUS_UNLOAD;
	
	return Status;
}
