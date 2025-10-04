/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	pci.h
	
Abstract:
	This header defines PCI-related structures and
	function prototypes.
	
Author:
	iProgramInCpp - 4 July 2024
***/
#pragma once

#include <stdint.h>
#include <ke.h>
#include <mm.h>
#include <hal.h> // includes hal/pci.h which defines certain types
#include <string.h>

#define PCI_CONFIG_ADDRESS (0xCF8)
#define PCI_CONFIG_DATA    (0xCFC)

#define PCI_VENDOR_INVALID (0xFFFF)

// PCI capability IDs.
#define PCI_CAP_PCI_EXPRESS (0x01)
#define PCI_CAP_MSI         (0x05)
#define PCI_CAP_MSI_X       (0x11)

#define PCI_MAX_BUS  (256)
#define PCI_MAX_SLOT (32)
#define PCI_MAX_FUNC (8)

#ifdef DEBUG2
#define PCIDEBUG
#endif

#ifdef PCIDEBUG
#define PciDbgPrint(...) DbgPrint(__VA_ARGS__)
#else
#define PciDbgPrint(...) do { } while (0)
#endif

void HalInitPci();

uint32_t HalPciConfigReadDword(PPCI_ADDRESS Address, uint8_t Offset);
uint16_t HalPciConfigReadWord(PPCI_ADDRESS Address, uint8_t Offset);
void HalPciConfigWriteDword(PPCI_ADDRESS Address, uint8_t Offset, uint32_t Data);
void HalPciReadDeviceIdentifier(PPCI_ADDRESS Address, PPCI_IDENTIFIER OutIdentifier);
uint32_t HalPciReadBar(PPCI_ADDRESS Address, int BarIndex);
uint32_t HalPciReadBarIoAddress(PPCI_ADDRESS Address, int BarIndex);
uintptr_t HalPciReadBarAddress(PPCI_ADDRESS Address, int BarIndex);