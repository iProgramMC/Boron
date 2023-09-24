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

_Static_assert(offsetof(RSDP_DESCRIPTION, Length) == 20, "RSDP 1 structure needs to be 20 bytes");

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

void HalInitAcpi();

#endif//BORON_HA_ACPI_H
