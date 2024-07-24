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

#include <mm/pmm.h>

//
// Offsets for base PCI configuration header.
//
#define PCI_OFFSET_DEVICE_IDENTIFIER  (0x00)  // vendor ID bits 0:15, device ID bits 16:31
#define PCI_OFFSET_STATUS_COMMAND     (0x04)  // command bits 0:15, status bits 16:31
#define PCI_OFFSET_REVISION_CLASS     (0x08)  // revision bits 0:7, prog IF bits 8:15, subclass bits 16:23, class bits 24:31
#define PCI_OFFSET_ACCESS_DETAILS     (0x0C)  // cache line size byte 0, latency timer byte 1, header type byte 2, BIST byte 3

//
// Offsets for PCI configuration header of type 0x0.
//
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
// PCI command flags.
//
#define PCI_CMD_IOSPACE          (1 << 0)
#define PCI_CMD_MEMORYSPACE      (1 << 1)
#define PCI_CMD_BUSMASTERING     (1 << 2)
#define PCI_CMD_SPECIALCYCLES    (1 << 3)
#define PCI_CMD_MEMWRINVENABLE   (1 << 4)
#define PCI_CMD_VGAPALETTESNOOP  (1 << 5)
#define PCI_CMD_PARERRRESPONSE   (1 << 6)
#define PCI_CMD_SERRNENABLE      (1 << 8)
#define PCI_CMD_FASTB2BENABLE    (1 << 9)
#define PCI_CMD_INTERRUPTDISABLE (1 << 10)

//
// PCI status flags.
//
#define PCI_STA_INTERRUPTSTATUS  (1 << 3)
#define PCI_STA_CAPABILITIESLIST (1 << 4)

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
// Holds information about this PCI device's MSI capability, if it
// exists.
//
typedef struct
{
	bool Exists;
	uint8_t CapabilityOffset;
}
PCI_MSI_DATA, *PPCI_MSI_DATA;

//
// Holds information about this PCI device's MSI-X capability, if it
// exists.
//
typedef struct
{
	bool Exists;
	uint8_t CapabilityOffset;
	uint8_t Bir;                  // Index into the BAR array used for the message table
	uint8_t Pbir;                 // Ditto but for the pending bit table
	int     TableSize;
	uintptr_t TableOffset;        // Offset into BAR(BIR)
	uintptr_t PendingTableOffset; // Offset into BAR(PBIR)
}
PCI_MSIX_DATA, *PPCI_MSIX_DATA;

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
	PCI_MSI_DATA MsiData;
	PCI_MSIX_DATA MsixData;
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

//
// MSI message control register flags.
//
#define PCI_MSI_MC_ENABLE            (1 << 0)
#define PCI_MSI_MC_MULTIMSGCAP(logn) (((logn) & 0x7) << 1) // Capable: logn - The log2() of the number of interrupts supported
#define PCI_MSI_MC_MULTIMSGEN(logn)  (((logn) & 0x7) << 4) // Enable:  logn - The log2() of the number of interrupts supported
#define PCI_MSI_MC_64BIT             (1 << 7)
#define PCI_MSI_MC_PERVECTORMASK     (1 << 8)

//
// MSI-X table entry.
//
typedef struct
{
	uint64_t Address;
	uint32_t MessageData;
	uint32_t VectorControl;
}
PCI_MSIX_TABLE_ENTRY, *PPCI_MSIX_TABLE_ENTRY;

//
// MSI-X message control bits.
//
#define PCI_MSIX_MC_ENABLE        (1 << 15)
#define PCI_MSIX_MC_FUNMASK       (1 << 14)
#define PCI_MSIX_MC_TABLESIZE(x)  ((x) & ((1<<10) - 1))

//
// MSI-X vector control bits.
//
#define PCI_MSIX_MASKED  (1 << 0) // VectorControl

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

FORCE_INLINE
void HalPciMsixSetFunctionMask(PPCI_DEVICE Device, bool Mask) {
	ASSERT(Device->MsixData.Exists);
	uint32_t U = HalPciConfigReadDword(
		&Device->Address,
		Device->MsixData.CapabilityOffset
	);
	
	if ((Mask))
		U |= (PCI_MSIX_MC_FUNMASK << 16);
	else
		U &= ~(PCI_MSIX_MC_FUNMASK << 16);
	
	HalPciConfigWriteDword(
		&Device->Address,
		Device->MsixData.CapabilityOffset,
		U
	);
}

//
// Creates a message used in an MSI vector configuration.  When written to one of the LAPICs in
// the system, it will trigger an interrupt on that processor.
//
FORCE_INLINE
uint32_t HalPciMsiCreateMessage(uint8_t Vector, bool EdgeTrigger, bool Deassert)
{
	uint32_t Value = Vector;
	if (!EdgeTrigger) Value |= MSI_TRIGGERLEVEL;
	if (!Deassert)    Value |= MSI_LEVELASSERT;
	return Value;
}

//
// Creates an address that can be used in an MSI vector configuration.  Writing the return value of
// HalPciMsiCreateMessage to this address as a 32-bit integer will trigger the chosen interrupt vector.
//
FORCE_INLINE
uintptr_t HalPciMsiCreateAddress(uint8_t ProcessorId)
{
	return 0xFEE00000 | ProcessorId << 12;
}

//
// Assign an MSI message to the specific vector for an MSI-X capable device.
//
// Parameters:
//   Device      - The device affected by this change
//   Index       - The index within the MSI-X vector table affected by this change.
//   Vector      - The interrupt vector triggered when this message signalled interrupt is fired
//   EdgeTrigger - 
//
FORCE_INLINE
void HalPciMsixSetInterrupt(PPCI_DEVICE Device, int Index, uint8_t ProcessorId, uint8_t Vector, bool EdgeTrigger, bool Deassert)
{
	uintptr_t Address = HalPciReadBarAddress(&Device->Address, Device->MsixData.Bir);
	
	PPCI_MSIX_TABLE_ENTRY Table = MmGetHHDMOffsetAddr(Address + Device->MsixData.TableOffset);
	Table += Index;
	
	Table->Address = HalPciMsiCreateAddress(ProcessorId);
	Table->MessageData = HalPciMsiCreateMessage(Vector, EdgeTrigger, Deassert);
	Table->VectorControl = 0;
}
