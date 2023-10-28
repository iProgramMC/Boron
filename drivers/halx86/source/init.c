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
#include <hal.h>
#include <ke.h>
#include <ex.h>
#include "acpi.h"
#include "apic.h"
#include "hpet.h"

void _HalEndOfInterrupt();
void _HalRequestInterruptInTicks(uint64_t Ticks);
void _HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void _HalInitSystemUP();
void _HalInitSystemMP();
void _HalDisplayString(const char* Message);
void _HalCrashSystem(const char* Message) NO_RETURN;
bool _HalUseOneShotIntTimer();
void _HalProcessorCrashed() NO_RETURN;
uint64_t _HalGetIntTimerFrequency();
uint64_t _HalGetTickCount();
uint64_t _HalGetTickFrequency();

static HAL_VFTABLE HalpVfTable;

PKHALCB KeGetCurrentHalCB()
{
	return KeGetCurrentPRCB()->HalData;
}

// Initialize the HAL on the BSP, for all processors.
void _HalInitSystemUP()
{
	HalInitTerminal();
	LogMsg("Boron (TM), October 2023 - V0.005");
	
	HalInitApicUP();
	HalInitAcpi();
	HalInitIoApic();
	HpetInitialize();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
void _HalInitSystemMP()
{
	KeGetCurrentPRCB()->HalData = ExAllocateSmall(sizeof(KHALCB));
	
	HalInitApicMP();
	HalCalibrateApic();
}

int DriverEntry()
{
	HalpVfTable.EndOfInterrupt = _HalEndOfInterrupt,
	HalpVfTable.RequestInterruptInTicks = _HalRequestInterruptInTicks,
	HalpVfTable.RequestIpi = _HalRequestIpi,
	HalpVfTable.InitSystemUP = _HalInitSystemUP,
	HalpVfTable.InitSystemMP = _HalInitSystemMP,
	HalpVfTable.DisplayString = _HalDisplayString,
	HalpVfTable.CrashSystem = _HalCrashSystem,
	HalpVfTable.ProcessorCrashed = _HalProcessorCrashed,
	HalpVfTable.UseOneShotIntTimer = _HalUseOneShotIntTimer,
	HalpVfTable.GetIntTimerFrequency = _HalGetIntTimerFrequency,
	HalpVfTable.GetTickCount = _HalGetTickCount,
	HalpVfTable.GetTickFrequency = _HalGetTickFrequency,
	HalpVfTable.Flags = HAL_VFTABLE_LOADED;
	
	// Hook the HAL's functions.
	HalSetVftable(&HalpVfTable);
	
	// And return, that's all we need.
	return 0;
}

