/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	hal/pci.h
	
Abstract:
	This header file defines the PCI related data types
	for the kernel-HAL interface.
	
Author:
	iProgramInCpp - 7 July 2024
***/
#pragma once

#include <stdint.h>
#include <status.h>

//
// Defines common PCI class codes.
//
enum
{
	PCI_CLASS_MASS_STORAGE = 1,
	PCI_CLASS_NETWORK,
	PCI_CLASS_DISPLAY,
	PCI_CLASS_MULTIMEDIA,
	PCI_CLASS_MEMORY,
	PCI_CLASS_BRIDGE,
	PCI_CLASS_COMMUNICATIONS,
	PCI_CLASS_BASE_PERIPHERAL,
	PCI_CLASS_INPUT_CONTROLLER,
	PCI_CLASS_SERIAL_BUS = 0xC,
};

//
// Defines common PCI subclass codes.
//
enum
{
	// Mass Storage Class (0x1)
	PCI_SUBCLASS_SCSI = 0x0,
	PCI_SUBCLASS_IDE,
	PCI_SUBCLASS_FLOPPY,
	PCI_SUBCLASS_ATA = 0x5,
	PCI_SUBCLASS_SATA,
	PCI_SUBCLASS_SASCSI,
	PCI_SUBCLASS_NVM,
};

//
// Defines a PCI device's geographical address - i.e. where
// the device is found on the PCI bus.
//
typedef struct
{
	uint8_t Bus;
	uint8_t Slot;
	uint8_t Function;
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

//
// Defines the structure of the PCI class register as shown in the
// PCI config table.
//
typedef union
{
	struct
	{
		uint8_t Revision;
		uint8_t ProgIF;
		uint8_t SubClass;
		uint8_t Class;
	};
	
	uint32_t Register;
}
PCI_CLASS, *PPCI_CLASS;

//
// Defines a registered PCI device.  Stores its device ID, vendor ID,
// class and subclass registers, as well as its geographical address
// on the bus.
//
typedef struct
{
	PCI_IDENTIFIER Identifier;
	PCI_ADDRESS Address;
	PCI_CLASS Class;
}
PCI_DEVICE, *PPCI_DEVICE;

//
// The function pointer type used by HalPciEnumerate.
//
typedef bool(*PHAL_PCI_ENUMERATE_CALLBACK)(PPCI_DEVICE Device, void* CallbackContext);

//
// Defined interface functions, along with their prototypes.
//
BSTATUS
HalPciEnumerate(
    bool LookUpByIds,
    size_t IdCount,
    PPCI_IDENTIFIER Identifiers,
    uint8_t ClassCode,
    uint8_t SubClassCode,
    PHAL_PCI_ENUMERATE_CALLBACK Callback,
	void* CallbackContext
);

typedef BSTATUS(*PFHAL_PCI_ENUMERATE)(bool, size_t, PPCI_IDENTIFIER, uint8_t, uint8_t, PHAL_PCI_ENUMERATE_CALLBACK, void*);

// Value that can be passed into HalPciEnumerate to specify that the subclass shouldn't be checked.
#define PCI_SUBCLASS_ANY (0xFF)

typedef uint32_t(*PFHAL_PCI_CONFIG_READ_DWORD)(PPCI_ADDRESS, uint8_t);
uint32_t HalPciConfigReadDword(PPCI_ADDRESS Address, uint8_t Offset);

typedef uint16_t(*PFHAL_PCI_CONFIG_READ_WORD)(PPCI_ADDRESS, uint8_t);
uint16_t HalPciConfigReadWord(PPCI_ADDRESS Address, uint8_t Offset);

typedef void(*PFHAL_PCI_CONFIG_WRITE_DWORD)(PPCI_ADDRESS, uint8_t, uint32_t);
void HalPciConfigWriteDword(PPCI_ADDRESS Address, uint8_t Offset, uint32_t Data);

typedef void(*PFHAL_PCI_READ_DEVICE_IDENTIFIER)(PPCI_ADDRESS, PPCI_IDENTIFIER);
void HalPciReadDeviceIdentifier(PPCI_ADDRESS Address, PPCI_IDENTIFIER OutIdentifier);

typedef uint32_t(*PFHAL_PCI_READ_BAR)(PPCI_ADDRESS, int);
uint32_t HalPciReadBar(PPCI_ADDRESS Address, int BarIndex);

typedef uint32_t(*PFHAL_PCI_READ_BAR_IO_ADDRESS)(PPCI_ADDRESS, int);
uint32_t HalPciReadBarIoAddress(PPCI_ADDRESS Address, int BarIndex);

typedef uintptr_t(*PFHAL_PCI_READ_BAR_ADDRESS)(PPCI_ADDRESS, int);
uintptr_t HalPciReadBarAddress(PPCI_ADDRESS Address, int BarIndex);
