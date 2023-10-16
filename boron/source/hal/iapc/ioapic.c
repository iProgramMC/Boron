/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/ioapic.c
	
Abstract:
	This module contains the implementation of the IOAPIC driver.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include <ke.h>
#include "acpi.h"
#include "apic.h"

extern PSDT_HEADER HalSdtApic;

static PMADT_LAPIC      HalpLApics    [256]; static int HalpLApicCount;
static PMADT_IOAPIC     HalpIoApics   [256]; static int HalpIoApicCount;
static PMADT_IOAPIC_ISO HalpIoApicIsos[256]; static int HalpIoApicIsoCount;
static PMADT_LAPIC_NMI  HalpLApicNmis [256]; static int HalpLApicNmiCount;

static uint32_t HalpIoApicRead(PMADT_IOAPIC IoApic, uint32_t Register)
{
	uintptr_t Base = (uintptr_t) MmGetHHDMOffsetAddr(IoApic->Address);
	
	*(uint32_t*)Base = Register;
	
	return *(uint32_t*)(Base + 16);
}

static void HalpIoApicWrite(PMADT_IOAPIC IoApic, uint32_t Register, uint32_t Value)
{
	uintptr_t Base = (uintptr_t) MmGetHHDMOffsetAddr(IoApic->Address);
	
	*(uint32_t*)Base = Register;
	*(uint32_t*)(Base + 16) = Value;
}

static int HalpIoApicGetGsiCount(PMADT_IOAPIC IoApic)
{
	return (HalpIoApicRead(IoApic, 0x1) & 0xFF0000) >> 16;
}

static PMADT_IOAPIC HalpGetIoApicFromGsi(uint32_t Gsi)
{
	for (int i = 0; i < HalpIoApicCount; i++)
	{
		PMADT_IOAPIC IoApic = HalpIoApics[i];
		
		if (IoApic->GsiBase <= Gsi && Gsi < IoApic->GsiBase + HalpIoApicGetGsiCount(IoApic))
			return IoApic;
	}
	
	SLogMsg("Error, cannot determine I/O APIC from GSI %d", Gsi);
	return NULL;
}

void HalIoApicSetGsiRedirect(uint8_t Vector, uint8_t Gsi, uint32_t LapicId, uint16_t Flags, bool Status)
{
	PMADT_IOAPIC IoApic = HalpGetIoApicFromGsi(Gsi);
	
	uint64_t Redirect = Vector;
	if ((Flags & (1 << 1)) != 0)
		Redirect |= 1 << 13;
	
	if ((Flags & (1 << 3)) != 0)
		Redirect |= 1 << 15;
	
	if (!Status)
		Redirect |= 1 << 16;
	
	Redirect |= (uint64_t) LapicId << 56;
	
	uint32_t IoRedirectTable = (Gsi - IoApic->GsiBase) * 2 + 16;
	HalpIoApicWrite(IoApic, IoRedirectTable + 0, (uint32_t) Redirect);
	HalpIoApicWrite(IoApic, IoRedirectTable + 1, (uint32_t)(Redirect >> 32));
}

void HalIoApicSetIrqRedirect(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status)
{
	for (int i = 0; i < HalpIoApicIsoCount; i++)
	{
		PMADT_IOAPIC_ISO Iso = HalpIoApicIsos[i];
		
		if (Iso->IrqSource != Irq)
			continue;
		
		HalIoApicSetGsiRedirect(Vector, Iso->Gsi, LapicId, Iso->Flags, Status);
		return;
	}
	
	HalIoApicSetGsiRedirect(Vector, Irq, LapicId, 0, Status);
}

void HalInitIoApic()
{
	PMADT_HEADER Madt = (PMADT_HEADER) HalSdtApic;
	
	if (!Madt)
	{
		LogMsg("Warning: The system does not have an MADT entry.");
		return;
	}
	
	int Offset = 0;
	
	while (true)
	{
		// If we can't read the MADT entry header tag
		if (Madt->Header.Length - sizeof *Madt - Offset < 2)
			break;
		
		PMADT_ENTRY_HEADER Header = (PMADT_ENTRY_HEADER)&Madt->EntryData[Offset];
		
		switch (Header->EntryType)
		{
			case MADT_ENTRY_LAPIC:
				SLogMsg("Madt: Found local APIC");
				HalpLApics[HalpLApicCount++] = (PMADT_LAPIC) Header;
				break;
			case MADT_ENTRY_IOAPIC:
				SLogMsg("Madt: Found IO APIC");
				HalpIoApics[HalpIoApicCount++] = (PMADT_IOAPIC) Header;
				break;
			case MADT_ENTRY_IOAPIC_ISO:
				SLogMsg("Madt: Found IO APIC ISO");
				HalpIoApicIsos[HalpIoApicIsoCount++] = (PMADT_IOAPIC_ISO) Header;
				break;
			case MADT_ENTRY_LAPIC_NMI:
				SLogMsg("Madt: Found local APIC NMI");
				HalpLApicNmis[HalpLApicNmiCount++] = (PMADT_LAPIC_NMI) Header;
				break;
		}
		
		int OffsetIncrement = Header->RecordLength;
		if (OffsetIncrement < 2)
			OffsetIncrement = 2;
		
		Offset += OffsetIncrement;
	}
}
