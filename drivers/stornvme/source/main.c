/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function for the
	NVMe device driver.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#include "nvme.h"
#include <string.h>

PDRIVER_OBJECT NvmeDriverObject;

IO_DISPATCH_TABLE NvmeDispatchTable;

void NvmeInitializeDispatchTable()
{
	
}

BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	NvmeDriverObject = DriverObject;
	
	NvmeInitializeDispatchTable();
	
	BSTATUS Status = HalPciEnumerate(
		false,
		0,
		NULL,
		PCI_CLASS_MASS_STORAGE,
		PCI_SUBCLASS_SATA,
		NvmePciDeviceEnumerated,
		NULL
	);
	
	if (Status == STATUS_NO_SUCH_DEVICES)
		Status =  STATUS_UNLOAD;
	
	return Status;
}
