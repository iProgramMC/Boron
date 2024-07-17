/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	pci.c
	
Abstract:
	This module defines the base PCI functions.
	
Author:
	iProgramInCpp - 4 July 2023
***/
#include "pio.h"
#include "pci.h"

static void PcipWriteAddress(PPCI_ADDRESS Address, uint8_t Offset)
{
	KePortWriteDword(
		PCI_CONFIG_ADDRESS,
		(Address->Bus      << 16) |
		(Address->Slot     << 11) |
		(Address->Function << 8)  |
		(Offset & 0xFC)           |
		0x80000000
	);
}

uint32_t HalPciConfigReadDword(PPCI_ADDRESS Address, uint8_t Offset)
{
	PcipWriteAddress(Address, Offset);
	return KePortReadDword(PCI_CONFIG_DATA);
}

void HalPciConfigWriteDword(PPCI_ADDRESS Address, uint8_t Offset, uint32_t Data)
{
	PcipWriteAddress(Address, Offset);
	KePortWriteDword(PCI_CONFIG_DATA, Data);
}

uint16_t HalPciConfigReadWord(PPCI_ADDRESS Address, uint8_t Offset)
{
	PcipWriteAddress(Address, Offset);
	return (uint16_t)(KePortReadDword(PCI_CONFIG_DATA) >> (8 * (Offset & 2)));
}

void HalPciReadDeviceIdentifier(PPCI_ADDRESS Address, PPCI_IDENTIFIER OutIdentifier)
{
	OutIdentifier->VendorAndDeviceId = HalPciConfigReadDword(Address, PCI_OFFSET_DEVICE_IDENTIFIER);
}

uint32_t HalPciReadBar(PPCI_ADDRESS Address, int BarIndex)
{
	return HalPciConfigReadDword(Address, PCI_OFFSET_BAR0 + 4 * BarIndex);
}

uintptr_t HalPciReadBarAddress(PPCI_ADDRESS Address, int BarIndex)
{
	uint32_t LowBar = HalPciReadBar(Address, BarIndex);
	
	// Check if the BAR is actually for an I/O space device.
	if (LowBar & 1)
		return 0;
	
	// Check the type of BAR.
	int Type = (int) (LowBar & 0b110) >> 1;
	
	// NOTE: This source file will probably be shared between the 32-bit and
	// 64-bit x86 HALs, explaining this check.
#ifndef IS_64_BIT
	if (Type == 2)
		Type = 0;
#endif
	
	if (Type == 0)
		// 32-bit BAR
		return LowBar & ~0xF;
	
	if (Type != 2)
		// Unknown type of BAR
		return 0;
	
	return ((uint64_t)HalPciReadBar(Address, BarIndex + 1) << 32) | (LowBar & ~0xF);
}

uint32_t HalPciReadBarIoAddress(PPCI_ADDRESS Address, int BarIndex)
{
	uint32_t Bar = HalPciReadBar(Address, BarIndex);
	
	// Check if the BAR is actually for a memory space device.
	if (~Bar & 1)
		return 0;
	
	return Bar & ~0x3;
}

PPCI_DEVICE HalpPciDevices;
size_t HalpPciDeviceCount, HalpPciDeviceCapacity;
KSPIN_LOCK HalpPciDeviceSpinLock;

static void HalpPciAddDevice(PPCI_DEVICE Device)
{
	// Process this device's capabilities list, if it exists.
	uint16_t Status = HalPciConfigReadWord(&Device->Address, PCI_OFFSET_STATUS_COMMAND + 2);
	
	if (Status & PCI_STA_CAPABILITIESLIST)
	{
		// The capabilities pointer is stored at offset 0x34.
		// Trim the upper 24 bits as the header is only 256 bytes in size, and also the lower 4 bits.
		uint32_t CapabilityOffset = HalPciConfigReadDword(&Device->Address, PCI_OFFSET_CAPABILITIES_PTR);
		CapabilityOffset &= 0xFC;
		
		// The capability offset linked list is terminated with a zero.
		while (CapabilityOffset != 0)
		{
			uint32_t UpperDword = HalPciConfigReadDword(&Device->Address, CapabilityOffset);
			uint8_t  Capability = UpperDword & 0xFF;
			
			// Determine the capability's ID.
			switch (Capability)
			{
				case PCI_CAP_MSI:
				{
					Device->MsiData.Exists = true;
					Device->MsiData.CapabilityOffset = CapabilityOffset;
					break;
				}
				
				case PCI_CAP_MSI_X:
				{
					Device->MsixData.Exists = true;
					Device->MsixData.CapabilityOffset = CapabilityOffset;
					break;
				}
				
				default:
				{
					DbgPrint(
						"PCI device %04x_%04x at %d.%d.%d has unrecognised capability %02x",
						Device->Identifier.VendorId,
						Device->Identifier.DeviceId,
						Device->Address.Bus,
						Device->Address.Slot,
						Device->Address.Function,
						Capability
					);
					break;
				}
			}
			
			// Jump to the next capability offset.
			CapabilityOffset = (UpperDword >> 8) & 0xFC;
		}
	}
	
	// Now, add it into the list of known PCI devices.
	KIPL Ipl;
	KeAcquireSpinLock(&HalpPciDeviceSpinLock, &Ipl);
	
	size_t Index;
	if ((Index = HalpPciDeviceCount++) == HalpPciDeviceCapacity)
	{
		size_t OldCapacity = HalpPciDeviceCapacity;
		
		if (HalpPciDeviceCapacity < 16)
			HalpPciDeviceCapacity = 16;
		else
			HalpPciDeviceCapacity *= 2;
		
		PPCI_DEVICE NewDevices = MmAllocatePool(POOL_NONPAGED, sizeof(PCI_DEVICE) * HalpPciDeviceCapacity);
		if (!NewDevices)
		{
			KeCrash(
				"WARNING: Could not add PCI device with address %d,%d,%d due to an out of memory condition",
				Device->Address.Bus,
				Device->Address.Slot,
				Device->Address.Function
			);
		}
		
		memset(NewDevices + OldCapacity, 0, sizeof(PCI_DEVICE) * (HalpPciDeviceCapacity - OldCapacity));
		
		if (HalpPciDevices)
		{
			memcpy(NewDevices, HalpPciDevices, sizeof(PCI_DEVICE) * OldCapacity);
			MmFreePool(HalpPciDevices);
		}
		HalpPciDevices = NewDevices;
	}
	
	HalpPciDevices[Index] = *Device;
	
	KeReleaseSpinLock(&HalpPciDeviceSpinLock, Ipl);
}

void HalPciProbe()
{
	DbgPrint("Probing PCI devices...");
	
	// TODO: Better way to probe devices.
	PCI_DEVICE Device;
	
	for (int Bus = 0; Bus < PCI_MAX_BUS; Bus++)
	{
		for (int Slot = 0; Slot < PCI_MAX_SLOT; Slot++)
		{
			for (int Function = 0; Function < PCI_MAX_FUNC; Function++)
			{
				memset(&Device, 0, sizeof Device);
				
				Device.Address.Bus = Bus;
				Device.Address.Slot = Slot;
				Device.Address.Function = Function;
				
				HalPciReadDeviceIdentifier(&Device.Address, &Device.Identifier);
				
				if (Device.Identifier.VendorId == 0xFFFF)
					continue;
				
				//DbgPrint("Found PCI device. Vendor ID: %04x, Device ID: %04x", Device.Identifier.VendorId, Device.Identifier.DeviceId);
				
				// Also fetch its class register
				Device.Class.Register = HalPciConfigReadDword(&Device.Address, PCI_OFFSET_REVISION_CLASS);
				
				HalpPciAddDevice(&Device);
			}
		}
	}
	
	// List all devices.
	KIPL Ipl;
	KeAcquireSpinLock(&HalpPciDeviceSpinLock, &Ipl);
	
	for (size_t i = 0; i < HalpPciDeviceCount; i++)
	{
		PPCI_DEVICE Device = &HalpPciDevices[i];
		
		DbgPrint(
			"DEVICE: Bus %02x, Slot %02x, Function %1x, Vendor ID: %04x, Device ID: %04x, Class: %02x, SubClass: %02x, ProgIF: %02x, Rev: %02x. Ints: %s",
			Device->Address.Bus,
			Device->Address.Slot,
			Device->Address.Function,
			Device->Identifier.VendorId,
			Device->Identifier.DeviceId,
			Device->Class.Class,
			Device->Class.SubClass,
			Device->Class.ProgIF,
			Device->Class.Revision,
			Device->MsixData.Exists ? "MSI-X" : (Device->MsiData.Exists ? "MSI" : "No Interrupts")
		);
	}
	
	KeReleaseSpinLock(&HalpPciDeviceSpinLock, Ipl);
	
	DbgPrint("PCI device probe complete.");
}

BSTATUS
HalPciEnumerate(
    bool LookUpByIds,
    size_t IdCount,
    PPCI_IDENTIFIER Identifiers,
    uint8_t ClassCode,
    uint8_t SubClassCode,
    PHAL_PCI_ENUMERATE_CALLBACK Callback,
	void* CallbackContext
)
{
	bool FoundDevices = false;
	
	for (size_t i = 0; i < HalpPciDeviceCount; i++)
	{
		PPCI_DEVICE Device = &HalpPciDevices[i];
		
		// Check if the device matches.
		if (LookUpByIds)
		{
			bool Is = false;
			
			for (size_t j = 0; !Is && j < IdCount; j++)
			{
				if (Device->Identifier.VendorAndDeviceId == Identifiers[j].VendorAndDeviceId)
					Is = true;
			}
			
			if (Is)
			{
				if (Callback(Device, CallbackContext))
					FoundDevices = true;
			}
		}
		else
		{
			if (ClassCode == Device->Class.Class &&
			   (SubClassCode == PCI_SUBCLASS_ANY || SubClassCode == Device->Class.SubClass))
			{
				if (Callback(Device, CallbackContext))
					FoundDevices = true;
			}
		}
	}
	
	if (FoundDevices)
		return STATUS_SUCCESS;
	else
		return STATUS_NO_SUCH_DEVICES;
}

void HalInitPci()
{
	HalPciProbe();
}