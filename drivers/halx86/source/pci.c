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

void HalInitPci()
{
	// TODO
}