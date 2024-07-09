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
	(void) Device;
	(void) CallbackContext;
	
	// The base address of the AHCI controller is located at BAR 5.
	//uintptr_t BaseAddress = PciReadBarAddress(&Device->Address, 5);
	
	// Use the HHDM offset to determine the HBA's 
	return true;
}

void AhciInitializeDispatchTable()
{
	
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	AhciDriverObject = DriverObject;
	
	AhciInitializeDispatchTable();
	
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
