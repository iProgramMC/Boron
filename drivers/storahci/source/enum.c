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
	
	// TODO: Check HbaMem->Ghc.PortsImplemented, etc.
	
	return true;
}
