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

bool NvmeSeekable(UNUSED PFCB Fcb)
{
	return true;
}

IO_DISPATCH_TABLE NvmeDispatchTable = {
	.Read = NvmeRead,
	.Write = NvmeWrite,
	.Seekable = NvmeSeekable,
	.GetAlignmentInfo = NvmeGetAlignmentInfo
};

int AllocateVector(PKIPL Ipl, KIPL Default)
{
	int Vector = -1;
	
	*Ipl = Default;
	while ((Vector = KeAllocateInterruptVector(*Ipl)) < 0 && *Ipl < IPL_CLOCK)
		(*Ipl)++;
	
	return Vector;
}

/*
void NvmeInitializeDispatchTable()
{
	NvmeDispatchTable.Read = NvmeRead;
	NvmeDispatchTable.Write = NvmeWrite;
	NvmeDispatchTable.Seekable = NvmeSeekable;
	NvmeDispatchTable.GetAlignmentInfo = NvmeGetAlignmentInfo;
}
*/
BSTATUS DriverEntry(PDRIVER_OBJECT DriverObject)
{
	NvmeDriverObject = DriverObject;
	
	//NvmeInitializeDispatchTable();
	
	BSTATUS Status = HalPciEnumerate(
		false,
		0,
		NULL,
		PCI_CLASS_MASS_STORAGE,
		PCI_SUBCLASS_NVM,
		NvmePciDeviceEnumerated,
		NULL
	);
	
	if (Status == STATUS_NO_SUCH_DEVICES)
		Status =  STATUS_UNLOAD;
	
	return Status;
}
