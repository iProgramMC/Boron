/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	pci.h
	
Abstract:
	This header defines PCI-related structures and
	function prototypes.
	
Author:
	iProgramInCpp - 4 July 2023
***/
#pragma once

#include <stdint.h>

#define PCI_CONFIG_ADDRESS (0xCF8)
#define PCI_CONFIG_DATA    (0xCFC)

#define PCI_VENDOR_INVALID (0xFFFF)

// base PCI configuration header
#define PCI_OFFSET_DEVICE_IDENTIFIER  (0x00)  // vendor ID bits 0:15, device ID bits 16:31
#define PCI_OFFSET_STATUS_COMMAND     (0x04)  // command bits 0:15, status bits 16:31
#define PCI_OFFSET_REVISION_CLASS     (0x08)  // revision bits 0:7, prog IF bits 8:15, subclass bits 16:23, class bits 24:31
#define PCI_OFFSET_ACCESS_DETAILS     (0x0C)  // cache line size byte 0, latency timer byte 1, header type byte 2, BIST byte 3

// header type 0x0
#define PCI_OFFSET_BAR0               (0x10)
#define PCI_OFFSET_BAR1               (0x14)
#define PCI_OFFSET_BAR2               (0x18)
#define PCI_OFFSET_BAR3               (0x1C)
#define PCI_OFFSET_BAR4               (0x20)
#define PCI_OFFSET_BAR5               (0x24)
#define PCI_OFFSET_SUBSYS_IDENTIFIER  (0x2C)
#define PCI_OFFSET_EXPROM_BASE_ADDR   (0x30)
#define PCI_OFFSET_CAPABILITIES_PTR   (0x34)  // byte 0 used, rest reserved
#define PCI_OFFSET_INTERRUPT          (0x3C)  // interrupt line byte 0, interrupt pin byte 1, min grant byte 2, max latency byte 3

//
// Defines a PCI device geographical address - i.e. where
// the device is found on the PCI bus.
//
typedef struct
{
	uint32_t Bus;
	uint32_t Slot;
	uint32_t Function;
}
PCI_ADDRESS, *PPCI_ADDRESS;

//
// Defines a PCI device identifier.  Is a group of two shorts
// called VendorId and DeviceId.  Laid out as stored in the PCI
// configuration space.
//
typedef union
{
	struct
	{
		uint16_t VendorId;
		uint16_t DeviceId;
	};
	
	uint32_t VendorAndDeviceId;
}
PCI_IDENTIFIER, *PPCI_IDENTIFIER;

void HalInitPci();
