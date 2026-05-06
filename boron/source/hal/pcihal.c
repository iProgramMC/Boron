/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/pcihal.c
	
Abstract:
	This module contains the implementation of the HAL dispatch
	functions for PCI enumeration & processing.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#include <hal.h>
#include <ke.h>

extern HAL_VFTABLE HalpVftable;

#if defined TARGET_AMD64 || defined TARGET_I386

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
	return HalpVftable.PciEnumerate(LookUpByIds, IdCount, Identifiers, ClassCode, SubClassCode, Callback, CallbackContext);
}

uint32_t HalPciConfigReadDword(PPCI_ADDRESS Address, uint8_t Offset)
{
	return HalpVftable.PciConfigReadDword(Address, Offset);
}

uint16_t HalPciConfigReadWord(PPCI_ADDRESS Address, uint8_t Offset)
{
	return HalpVftable.PciConfigReadWord(Address, Offset);
}

void HalPciConfigWriteDword(PPCI_ADDRESS Address, uint8_t Offset, uint32_t Data)
{
	HalpVftable.PciConfigWriteDword(Address, Offset, Data);
}

void HalPciReadDeviceIdentifier(PPCI_ADDRESS Address, PPCI_IDENTIFIER OutIdentifier)
{
	HalpVftable.PciReadDeviceIdentifier(Address, OutIdentifier);
}

uint32_t HalPciReadBar(PPCI_ADDRESS Address, int BarIndex)
{
	return HalpVftable.PciReadBar(Address, BarIndex);
}

uint32_t HalPciReadBarIoAddress(PPCI_ADDRESS Address, int BarIndex)
{
	return HalpVftable.PciReadBarIoAddress(Address, BarIndex);
}

uintptr_t HalPciReadBarAddress(PPCI_ADDRESS Address, int BarIndex)
{
	return HalpVftable.PciReadBarAddress(Address, BarIndex);
}

#endif
