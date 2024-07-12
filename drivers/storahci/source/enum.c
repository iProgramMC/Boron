/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	enum.c
	
Abstract:
	This module implements PCI device enumeration logic
	for the AHCI disk driver.
	
Author:
	iProgramInCpp - 10 July 2024
***/
#include "ahci.h"
#include <string.h>

int ControllerNumber; // Atomically incremented
int DriveNumber; // TODO: Maybe a system wide solution?

// TODO: Better way of identifying devices?
// PxSIG seems to have a format different than you might expect:
//   31:24 - LBA high register
//   23:16 - LBA mid register
//   15:08 - LBA low register
//   07:00 - sector count register
// I don't know how these could possibly line up with being unique identifiers for certain devices.
// I will use this method because it served NanoShell well so far, but we'll need to look for a better
// solution or a clarification as to how this method works.
#define	SATA_SIG_ATA    0x00000101  // SATA drive
#define	SATA_SIG_ATAPI  0xEB140101  // SATAPI drive
#define	SATA_SIG_SEMB   0xC33C0101  // Enclosure management bridge
#define	SATA_SIG_PM     0x96690101  // Port multiplier

void AhciRegisterDevice(PCONTROLLER_OBJECT Controller, const char* ControllerName, PAHCI_PORT Port)
{
	PCONTROLLER_EXTENSION ContExtension = (PCONTROLLER_EXTENSION) Controller->Extension;
	int Offset = (int)(Port - ContExtension->MemRegs->PortList);
	
	uint32_t Signature = Port->Signature;
	
	DbgPrint("Controller %s port %d has signature 0x%08X", OBJECT_GET_HEADER(Controller)->ObjectName, Offset, Signature);
	
	// TODO: only allow ATA devices for now
	if (Signature != SATA_SIG_ATA)
		return;
	
	char Buffer[32];
	snprintf(Buffer, sizeof Buffer, "%sDisk%d", ControllerName, Offset);
	
	// Create a device object for this device.
	PDEVICE_OBJECT DeviceObject;
	
	BSTATUS Status = IoCreateDevice(
		AhciDriverObject,
		sizeof(DEVICE_EXTENSION),
		sizeof(FCB_EXTENSION),
		Buffer,
		DEVICE_TYPE_BLOCK,
		false,
		&AhciDispatchTable,
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
		Offset,
		DeviceObject
	);
	
	if (FAILED(Status))
		KeCrash("StorAhci: Device #%d was already added to controller", Offset);
	
	// Initialize the specific device object.
	PDEVICE_EXTENSION DeviceExt = (PDEVICE_EXTENSION) DeviceObject->Extension;
	
	DeviceExt->ParentHba = ContExtension->MemRegs;
	DeviceExt->AhciPort = Port;
	
	
	// After the device object was initialized, de-reference it.
	ObDereferenceObject(DeviceObject);
}

bool AhciPciDeviceEnumerated(PPCI_DEVICE Device, UNUSED void* CallbackContext)
{
	int ContNumber = AtFetchAdd(ControllerNumber, 1);
	char Buffer[16];
	snprintf(Buffer, sizeof Buffer, "Ahci%d", ContNumber);
	PCONTROLLER_OBJECT Controller = NULL;
	BSTATUS Status = IoCreateController(
		AhciDriverObject,
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
	
	// The base address of the AHCI controller is located at BAR 5.
	// According to the specification, the lower 12 bits are reserved, so they are ignored.
	uintptr_t BaseAddress = HalPciReadBarAddress(&Device->Address, 5) & ~0xFFF;
	
	// Use the HHDM offset to determine the HBA's address in virtual memory.
	PAHCI_MEMORY_REGISTERS HbaMem = ContExtension->MemRegs = MmGetHHDMOffsetAddr(BaseAddress);
	
	// Set GHC.AE to 1 to disable legacy mechanisms of communication with the HBA.
	AtOrFetch(HbaMem->Ghc.GlobalHostControl, HBA_GHC_AE);
	
	// Determine the maximum amount of commands available.
	ContExtension->NumCommandSlots = (HbaMem->Ghc.HostCapabilities >> 8) & 0x1F;
	
	// If needed, perform BIOS/OS handoff.
	if (HbaMem->Ghc.HostCapabilities2 & HBA_CAPS2_BOH)
	{
		DbgPrint("BIOS/OS Handoff supported, so performing it");
		
		AtOrFetch(HbaMem->Ghc.BiosOsHandOff, HBA_BOHC_OOS);
		// ^ This will cause an SMI triggering the BIOS to perform necessary cleanup.
		
		// Spin on the BIOS Ownership bit, waiting for it to be cleared to zero.
		while (AtLoad(HbaMem->Ghc.BiosOsHandOff) & HBA_BOHC_BOS)
			KeSpinningHint();
		
		// Wait 25 ms, and check the BB bit.
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, 25, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
		
		// If after 25 ms, the BIOS Busy bit has not been set to 1, then the OS may assume control
		if (~AtLoad(HbaMem->Ghc.BiosOsHandOff) & HBA_BOHC_BB)
		{
			DbgPrint("Have to wait an additional 2 seconds for the BIOS to clean up");
			
			// The OS driver shall provide the BIOS a minimum of two seconds for finishing
			// outstanding commands on the HBA.
			//
			// Just poll every 100 ms, after two seconds, I think it will be fine
			KeInitializeTimer(&Timer);
			KeSetTimer(&Timer, 2000, NULL);
			KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
			
			while (~AtLoad(HbaMem->Ghc.BiosOsHandOff) & HBA_BOHC_BB)
			{
				KeInitializeTimer(&Timer);
				KeSetTimer(&Timer, 100, NULL);
				KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
			}
		}
	}
	
	uint32_t Pi = HbaMem->Ghc.PortsImplemented;
	
	for (int Port = 0; Port < 32; Port++)
	{
		if (~Pi & (1 << Port))
			continue;
		
		// This port is implemented.  Therefore, this is a device we can register.
		
		// First, check whether this port is actually open.
		PAHCI_PORT AhciPort = &HbaMem->PortList[Port];
		
		if (AhciPort->SataStatus.DeviceDetection != PORT_SSTS_DET_DETEST)
			// Device presence wasn't detected and/or Phy communication wasn't established
			continue;
		
		if (AhciPort->SataStatus.InterfacePowerManagement != PORT_SSTS_IPM_ACTIVE)
			// Interface is not in active state
			continue;
		
		// Register the device on the port now.
		AhciRegisterDevice(Controller, Buffer, AhciPort);
	}
	
	// After initializing the controller object, dereference it.
	ObDereferenceObject(Controller);
	return true;
}
 