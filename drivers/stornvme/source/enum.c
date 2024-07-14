/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	enum.c
	
Abstract:
	This module implements PCI device enumeration logic
	for the NVMe disk driver.
	
Author:
	iProgramInCpp - 10 July 2024
***/
#include "nvme.h"
#include <string.h>

int ControllerNumber; // Atomically incremented
int DriveNumber; // TODO: Maybe a system wide solution?

void NvmeRegisterDevice(PCONTROLLER_OBJECT Controller, const char* ControllerName, int Number)
{
	PCONTROLLER_EXTENSION ContExtension = (PCONTROLLER_EXTENSION) Controller->Extension;
	
	char Buffer[32];
	snprintf(Buffer, sizeof Buffer, "%sDisk%d", ControllerName, Number);
	
	// Create a device object for this device.
	PDEVICE_OBJECT DeviceObject;
	
	BSTATUS Status = IoCreateDevice(
		NvmeDriverObject,
		sizeof(DEVICE_EXTENSION),
		sizeof(FCB_EXTENSION),
		Buffer,
		DEVICE_TYPE_BLOCK,
		false,
		&NvmeDispatchTable,
		&DeviceObject
	);
	
	if (FAILED(Status))
	{
		DbgPrint("Cannot create device object called %s: %d", Buffer, Status);
		return;
	}
	
	// Add it to the controller.
	Status = IoAddDeviceController(
		Controller,
		Number,
		DeviceObject
	);
	
	if (FAILED(Status))
		KeCrash("StorNvme: Device #%d was already added to controller", Number);
	
	// Initialize the specific device object.
	
	// TODO
	
	// After the device object was initialized, de-reference it.
	ObDereferenceObject(DeviceObject);
}

bool NvmePciDeviceEnumerated(PPCI_DEVICE Device, UNUSED void* CallbackContext)
{
	int ContNumber = AtFetchAdd(ControllerNumber, 1);
	char Buffer[16];
	snprintf(Buffer, sizeof Buffer, "Nvme%d", ContNumber);
	PCONTROLLER_OBJECT Controller = NULL;
	BSTATUS Status = IoCreateController(
		NvmeDriverObject,
		sizeof(CONTROLLER_EXTENSION),
		Buffer,
		false,
		&Controller
	);
	
	if (FAILED(Status))
	{
		LogMsg("Could not create controller object for %s: status %d", Buffer, Status);
		return false;
	}
	
	PCONTROLLER_EXTENSION ContExtension = (PCONTROLLER_EXTENSION) Controller->Extension;
	
	ContExtension->PciDevice = Device;
	
	// The base address of the NVMe controller is located at BAR 0.
	// According to the specification, the lower 12 bits are reserved, so they are ignored.
	uintptr_t BaseAddress = HalPciReadBarAddress(&Device->Address, 0);
	
	// Use the HHDM offset to determine the address in virtual memory.
	void* ControllerData = MmGetHHDMOffsetAddr(BaseAddress);
	
	// TODO
	
	// After initializing the controller object, dereference it.
	ObDereferenceObject(Controller);
	return true;
}
 