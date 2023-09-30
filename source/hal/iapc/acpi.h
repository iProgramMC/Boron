/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/acpi.h
	
Abstract:
	This module is the implementation header for ACPI.
	
Author:
	iProgramInCpp - 23 September 2023
***/

#ifndef BORON_HA_ACPI_H
#define BORON_HA_ACPI_H

#include <main.h>
#include <limreq.h>
#include <string.h>
#include <mm.h>

typedef struct RSDP_DESCRIPTION_tag
{
	char     Signature[8];
	uint8_t  Checksum;
	char     OemId[6];
	uint8_t  Revision;
	uint32_t RsdtAddress; // physical address
	// RSDP 2
	uint32_t Length;
	uint64_t XsdtAddress; // physical address
	uint8_t  ChecksumExt;
	uint8_t  Reserved[3];
}
PACKED
RSDP_DESCRIPTION, *PRSDP_DESCRIPTION;

static_assert(offsetof(RSDP_DESCRIPTION, Length) == 20, "RSDP 1 structure needs to be 20 bytes");

typedef struct SDT_HEADER_tag
{
	char     Signature[4];
	uint32_t Length;
	uint8_t  Revision;
	uint8_t  Checksum;
	char     OemId[6];
	char     OemTableId[8];
	uint32_t OemRevision;
	uint32_t CreatorId;
	uint32_t CreatorRevision;
}
PACKED
SDT_HEADER, *PSDT_HEADER;

typedef struct RSDT_TABLE_tag
{
	SDT_HEADER Header;
	
	// note: These are actually flexible array members.
	// However, the compiler doesn't actually allow them, so just
	// set their size to zero.
	union {
		uint32_t Sdts32[0]; // List of physical addresses.
		uint64_t Sdts64[0]; // In case we're using the XSDT
	};
}
PACKED
RSDT_TABLE, *PRSDT_TABLE;

typedef struct MADT_HEADER_tag
{
	SDT_HEADER Header;     // base SDT header
	uint32_t LapicAddress; // address of the LAPIC. Also found in an MSR
	uint32_t Flags;        // Bit 0 set - legacy 8259 PIC is installed.
}
PACKED
MADT_HEADER, *PMADT_HEADER;

typedef struct MADT_ENTRY_HEADER_tag
{
	uint8_t EntryType;
	uint8_t RecordLength;
}
PACKED
MADT_ENTRY_HEADER;

typedef enum MADT_ENTRY_TYPE_tag
{
	MADT_ENTRY_LAPIC,
	MADT_ENTRY_IOAPIC,
	MADT_ENTRY_IOAPIC_ISO,
	MADT_ENTRY_IOAPIC_NMI,
	MADT_ENTRY_LAPIC_NMI,
	MADT_ENTRY_LAPIC_AO,
	
}
MADT_ENTRY_TYPE;

// ACPI generic address structure's address space
typedef enum ACPI_GAS_ASP_tag
{
	ACPI_ASP_MEMORY,
	ACPI_ASP_PIO,
}
ACPI_GAS_ASP;

// ACPI generic address structure
typedef struct ACPI_GAS_tag
{
	uint8_t  AddressSpace;
	uint8_t  BitWidth;
	uint8_t  BitOffset;
	uint8_t  AccessSize;
	uint64_t Address;
}
PACKED
ACPI_GAS, *PACPI_GAS;

typedef struct FADT_HEADER_tag
{
	SDT_HEADER Header;
	uint32_t FirmwareCtrl;
	uint32_t DsdtAddress;
	
	// Field used in ACPI 1.0; no longer in use, for compatibility only
	uint8_t  Reserved;
	
	uint8_t  PreferredPowerManagementProfile;
	uint16_t SCI_Interrupt;
	uint32_t SMI_CommandPort;
	uint8_t  AcpiEnable;
	uint8_t  AcpiDisable;
	uint8_t  S4BIOS_REQ;
	uint8_t  PSTATE_Control;
	uint32_t PM1aEventBlock;
	uint32_t PM1bEventBlock;
	uint32_t PM1aControlBlock;
	uint32_t PM1bControlBlock;
	uint32_t PM2ControlBlock;
	uint32_t PMTimerBlock;
	uint32_t GPE0Block;
	uint32_t GPE1Block;
	uint8_t  PM1EventLength;
	uint8_t  PM1ControlLength;
	uint8_t  PM2ControlLength;
	uint8_t  PMTimerLength;
	uint8_t  GPE0Length;
	uint8_t  GPE1Length;
	uint8_t  GPE1Base;
	uint8_t  CStateControl;
	uint16_t WorstC2Latency;
	uint16_t WorstC3Latency;
	uint16_t FlushSize;
	uint16_t FlushStride;
	uint8_t  DutyOffset;
	uint8_t  DutyWidth;
	uint8_t  DayAlarm;
	uint8_t  MonthAlarm;
	uint8_t  Century;
	
	uint16_t BootArchitectureFlags;
	
	uint8_t  Reserved2;
	uint32_t Flags;
	
	ACPI_GAS ResetReg;
	
	uint8_t  ResetValue;
	uint8_t  Reserved3[3];
	
	uint64_t X_FirmwareControl;
	uint64_t X_Dsdt;
	
	ACPI_GAS X_PM1aEventBlock;
	ACPI_GAS X_PM1bEventBlock;
	ACPI_GAS X_PM1aControlBlock;
	ACPI_GAS X_PM1bControlBlock;
	ACPI_GAS X_PM2ControlBlock;
	ACPI_GAS X_PMTimerBlock;
	ACPI_GAS X_GPE0Block;
	ACPI_GAS X_GPE1Block;
}
PACKED
FADT_HEADER, *PFADT_HEADER;

typedef struct HPET_HEADER_tag
{
	SDT_HEADER Header;
	uint8_t    HardwareRevId;
	uint8_t    ComparatorCount   : 5;
	uint8_t    CounterSize       : 1;
	uint8_t    Reserved          : 1;
	uint8_t    LegacyReplacement : 1;
	uint16_t   PciVendorId;
	ACPI_GAS   Address;
	uint8_t    HpetNumber;
	uint16_t   MinimumTick;
	uint8_t    PageProtection;
}
HPET_HEADER, *PHPET_HEADER;

void HalInitAcpi();

void HalInitIoApic();

bool HalAcpiIsPmtAvailable();

uint32_t HalGetPmtCounter();

void HalSetPmtCounter(uint32_t Value);

#endif//BORON_HA_ACPI_H
