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

uint32_t PciConfigReadDword(PPCI_ADDRESS Address, uint8_t Offset)
{
	PcipWriteAddress(Address, Offset);
	return KePortReadDword(PCI_CONFIG_DATA);
}

void PciConfigWriteDword(PPCI_ADDRESS Address, uint8_t Offset, uint32_t Data)
{
	PcipWriteAddress(Address, Offset);
	KePortWriteDword(PCI_CONFIG_DATA, Data);
}

uint16_t PciConfigReadWord(PPCI_ADDRESS Address, uint8_t Offset)
{
	PcipWriteAddress(Address, Offset);
	return (uint16_t)(KePortReadDword(PCI_CONFIG_DATA) >> (8 * (Offset & 2)));
}

void PciReadDeviceIdentifier(PPCI_ADDRESS Address, PPCI_IDENTIFIER OutIdentifier)
{
	OutIdentifier->VendorAndDeviceId = PciConfigReadDword(Address, PCI_OFFSET_DEVICE_IDENTIFIER);
}

uint32_t PciReadBar(PPCI_ADDRESS Address, int BarIndex)
{
	return PciConfigReadDword(Address, PCI_OFFSET_BAR0 + 4 * BarIndex);
}

uintptr_t PciReadBarAddress(PPCI_ADDRESS Address, int BarIndex)
{
	uint32_t LowBar = PciReadBar(Address, BarIndex);
	
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
	
	return ((uint64_t)PciReadBar(Address, BarIndex + 1) << 32) | (LowBar & ~0xF);
}

uint32_t PciReadBarIoAddress(PPCI_ADDRESS Address, int BarIndex)
{
	uint32_t Bar = PciReadBar(Address, BarIndex);
	
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
		Device.Address.Bus = Bus;
		for (Device.Address.Slot = 0; Device.Address.Slot < PCI_MAX_SLOT; Device.Address.Slot++)
		{
			for (Device.Address.Function = 0; Device.Address.Function < PCI_MAX_FUNC; Device.Address.Function++)
			{
				PciReadDeviceIdentifier(&Device.Address, &Device.Identifier);
				
				if (Device.Identifier.VendorId == 0xFFFF)
					continue;
				
				//DbgPrint("Found PCI device. Vendor ID: %04x, Device ID: %04x", Device.Identifier.VendorId, Device.Identifier.DeviceId);
				
				// Also fetch its class register
				Device.Class.Register = PciConfigReadDword(&Device.Address, PCI_OFFSET_REVISION_CLASS);
				
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
			"DEVICE: Bus %02x, Slot %02x, Function %1x, Vendor ID: %04x, Device ID: %04x, Class: %02x, SubClass: %02x, ProgIF: %02x, Rev: %02x",
			Device->Address.Bus,
			Device->Address.Slot,
			Device->Address.Function,
			Device->Identifier.VendorId,
			Device->Identifier.DeviceId,
			Device->Class.Class,
			Device->Class.SubClass,
			Device->Class.ProgIF,
			Device->Class.Revision
		);
	}
	
	KeReleaseSpinLock(&HalpPciDeviceSpinLock, Ipl);
	
	DbgPrint("PCI device probe complete.");
}

void HalInitPci()
{
	HalPciProbe();
}