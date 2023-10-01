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
		SLogMsg("HpetInitialize: There is no HPET installed.");
		return;
	}
	
	PHPET_HEADER Hpet = (PHPET_HEADER)HalSdtHpet;
	
	if (Hpet->Address.AddressSpace != ACPI_ASP_MEMORY || Hpet->Address.Address == 0)
		KeCrashBeforeSMPInit("Error, HPET isn't mapped in memory");
	
	uintptr_t HpetAddress = Hpet->Address.Address;
	
	// Map the HPET as uncacheable.
	void* Address;
	EXMEMORY_HANDLE Handle = ExAllocatePool(POOL_FLAG_USER_CONTROLLED, 1, &Address, EX_TAG("HPET"));
	if (!Handle)
	{
	CRASH_BECAUSE_FAILURE_TO_MAP:
		KeCrashBeforeSMPInit("Could not map HPET as uncacheable");
	}
	
	if (!MmMapPhysicalPage(MmGetCurrentPageMap(),
						   HpetAddress,
						   (uintptr_t) Address,
						   MM_PTE_READWRITE | MM_PTE_SUPERVISOR | MM_PTE_CDISABLE | MM_PTE_GLOBAL | MM_PTE_NOEXEC))
	{
	   goto CRASH_BECAUSE_FAILURE_TO_MAP;
	}
	
	HpetpIsAvailable = true;
	
	HpetpRegisters = (PHPET_REGISTERS) ((uintptr_t) Address + (HpetAddress & 0xFFF));
	
	HpetGeneralCaps.Contents = HpetpRegisters->GeneralCaps;
	
	LogMsg("HPET Capabilities: %p, Vendor ID: %04x",
	       HpetGeneralCaps.Contents,
	       HpetGeneralCaps.VendorID);
	
	LogMsg("Counter Clock Period: %d Femtoseconds per Tick (%d Nanoseconds)",
	       HpetGeneralCaps.CounterClockPeriod,
	       HpetGeneralCaps.CounterClockPeriod / 1000);
	
	// Enable and reset the main counter
	HpetpRegisters->GeneralConfig = 0;
	HpetpRegisters->CounterValue  = 0;
	HpetpRegisters->GeneralConfig = HPET_GEN_CFG_ENABLE_CNF;
	
	// Not sure why, but NanoShell64 tests for consistency.  We won't do that here.
}
