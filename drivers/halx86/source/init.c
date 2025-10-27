/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/init.c
	
Abstract:
	This module contains the two initialization functions
	(UP-init and MP-init)
	
Author:
	iProgramInCpp - 23 September 2023
***/
#include "hali.h"
#include <ke.h>
#include <ex.h>
#include "acpi.h"
#include "apic.h"
#include "hpet.h"
#include "ioapic.h"
#include "pci.h"

void HalEndOfInterrupt(int InterruptNumber);
void HalRequestInterruptInTicks(uint64_t Ticks);
void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void HalInitSystemUP();
void HalInitSystemMP();
void HalDisplayString(const char* Message);
void HalCrashSystem(const char* Message) NO_RETURN;
bool HalUseOneShotIntTimer();
void HalProcessorCrashed() NO_RETURN;
uint64_t HalGetIntTimerFrequency();
uint64_t HalGetTickCount();
uint64_t HalGetTickFrequency();
uint64_t HalGetIntTimerDeltaTicks();

static const HAL_VFTABLE HalpVfTable =
{
	.EndOfInterrupt = HalEndOfInterrupt,
	.RequestInterruptInTicks = HalRequestInterruptInTicks,
	.RequestIpi = HalRequestIpi,
	.InitSystemUP = HalInitSystemUP,
	.InitSystemMP = HalInitSystemMP,
	.DisplayString = HalDisplayString,
	.CrashSystem = HalCrashSystem,
	.ProcessorCrashed = HalProcessorCrashed,
	.UseOneShotIntTimer = HalUseOneShotIntTimer,
	.GetIntTimerFrequency = HalGetIntTimerFrequency,
	.GetTickCount = HalGetTickCount,
	.GetTickFrequency = HalGetTickFrequency,
	.GetIntTimerDeltaTicks = HalGetIntTimerDeltaTicks,
	.IoApicSetIrqRedirect = HalIoApicSetIrqRedirect,
	.PciEnumerate = HalPciEnumerate,
	.PciConfigReadDword = HalPciConfigReadDword,
	.PciConfigReadWord = HalPciConfigReadWord,
	.PciConfigWriteDword = HalPciConfigWriteDword,
	.PciReadDeviceIdentifier = HalPciReadDeviceIdentifier,
	.PciReadBar = HalPciReadBar,
	.PciReadBarAddress = HalPciReadBarAddress,
	.PciReadBarIoAddress = HalPciReadBarIoAddress,
	.Flags = HAL_VFTABLE_LOADED,
};

PKHALCB KeGetCurrentHalCB()
{
	return KeGetCurrentPRCB()->HalData;
}

// Initialize the HAL on the BSP, for all processors.
void HalInitSystemUP()
{
	HalInitTerminal();
	HalInitApicUP();
	HalInitAcpi();
	HalInitIoApic();
	HpetInitialize();
	HalInitPci();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
void HalInitSystemMP()
{
	KeGetCurrentPRCB()->HalData = MmAllocatePool(POOL_FLAG_NON_PAGED, sizeof(KHALCB));
	
	HalInitApicMP();
	HalCalibrateApic();
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	// Note! The HAL's driver object is kind of useless, it doesn't do anything actually.
	
	// Hook the HAL's functions.
	HalSetVftable(&HalpVfTable);
	
	// And return, that's all we need.
	return STATUS_SUCCESS;
}

