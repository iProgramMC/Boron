/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	nvme.h
	
Abstract:
	This header defines data types and includes common system
	headers used by the NVMe storage device driver.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#include <main.h>
#include <ke.h>
#include <io.h>
#include <hal.h>
#include <mm.h>

typedef struct
{
	PPCI_DEVICE PciDevice;
}
CONTROLLER_EXTENSION, *PCONTROLLER_EXTENSION;

typedef struct
{
	int Test;
}
DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct
{
	int Test;
}
FCB_EXTENSION, *PFCB_EXTENSION;

extern PDRIVER_OBJECT NvmeDriverObject;
extern IO_DISPATCH_TABLE NvmeDispatchTable;

bool NvmePciDeviceEnumerated(PPCI_DEVICE Device, void* CallbackContext);
