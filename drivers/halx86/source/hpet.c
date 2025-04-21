/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/hpet.c
	
Abstract:
	This module contains the implementation of the HPET
	timer driver for the IA-PC HAL.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#include <hal.h>
#include <mm.h>
#include <ex.h>
#include <ke.h>
#include "acpi.h"
#include "hpet.h"

static bool HpetpIsAvailable = false;

extern PRSDT_TABLE HalSdtHpet;

static PHPET_REGISTERS   HpetpRegisters;
static HPET_GENERAL_CAPS HpetGeneralCaps;

#define HPET_GEN_CFG_ENABLE_CNF (1 << 0)

bool HpetIsAvailable()
{
	return HpetpIsAvailable;
}

void HpetInitialize()
{
	if (!HalSdtHpet)
	{
		DbgPrint("HpetInitialize: There is no HPET installed.");
		return;
	}
	
	PHPET_HEADER Hpet = (PHPET_HEADER)HalSdtHpet;
	
	if (Hpet->Address.AddressSpace != ACPI_ASP_MEMORY || Hpet->Address.Address == 0)
		KeCrashBeforeSMPInit("Error, HPET isn't mapped in memory");
	
	uintptr_t HpetAddress = Hpet->Address.Address;
	
	// Map the HPET as uncacheable.
	void* Address = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, 1, POOL_TAG("HPET"));
	if (!Address)
	{
	CRASH_BECAUSE_FAILURE_TO_MAP:
		KeCrashBeforeSMPInit("Could not map HPET as uncacheable");
	}
	
	if (!MiMapPhysicalPage(MiGetCurrentPageMap(),
						   HpetAddress,
						   (uintptr_t) Address,
						   MM_PTE_READWRITE | MM_PTE_CDISABLE | MM_PTE_GLOBAL | MM_PTE_NOEXEC))
	{
	   goto CRASH_BECAUSE_FAILURE_TO_MAP;
	}
	
	HpetpIsAvailable = true;
	
	HpetpRegisters = (PHPET_REGISTERS) ((uintptr_t) Address + (HpetAddress & 0xFFF));
	
	HpetGeneralCaps.Contents = HpetpRegisters->GeneralCaps;
	
#ifdef DEBUG
	DbgPrint("HPET Capabilities: %p, Vendor ID: %04x",
	         HpetGeneralCaps.Contents,
	         HpetGeneralCaps.VendorID);
	
	DbgPrint("Counter Clock Period: %d Femtoseconds per Tick (%d Nanoseconds)",
	         HpetGeneralCaps.CounterClockPeriod,
	         HpetGeneralCaps.CounterClockPeriod / 1000);
#endif
	
	// Enable and reset the main counter
	HpetpRegisters->GeneralConfig = 0;
	HpetpRegisters->CounterValue  = 0;
	HpetpRegisters->GeneralConfig = HPET_GEN_CFG_ENABLE_CNF;
	
	// Not sure why, but NanoShell64 tests for consistency.  We won't do that here.
}

uint64_t HpetReadValue()
{
	return HpetpRegisters->CounterValue;
}

uint64_t HpetGetFrequency()
{
	uint64_t Period = HpetGeneralCaps.CounterClockPeriod; // In femtoseconds
	
	uint64_t Frequency = 1000000000000000ULL / Period;
	
	return Frequency;
}
