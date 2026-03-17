/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/init.c
	
Abstract:
	This module contains the two initialization functions
	(UP-init and MP-init) for the iPod touch 1G HAL.
	
Author:
	iProgramInCpp - 14 March 2026
***/
#include <ke.h>
#include <io.h>
#include "hali.h"

void HalEndOfInterrupt(int InterruptNumber);
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
int HalGetMaximumInterruptCount();
void HalOnUpdateIpl(KIPL NewIpl, KIPL OldIpl);
void HalVicRegisterInterrupt(int InterruptNumber, KIPL Ipl);
void HalVicDeregisterInterrupt(int InterruptNumber, KIPL Ipl);
PKREGISTERS HalOnInterruptRequest(PKREGISTERS Registers);
PKREGISTERS HalOnFastInterruptRequest(PKREGISTERS Registers);
bool HalUseOneShotTimer();
void HalRequestInterruptInTicks(uint64_t ticks);
uint64_t HalGetInterruptDeltaTime();
void HalInitCLCD();
void HalInitTerminal();
void HalInitPL192();
void HalInitClock();
void HalInitTimer();

// Initialize the HAL on the BSP, for all processors.
HAL_API void HalInitSystemUP()
{
	HalInitCLCD();
	HalInitTerminal();
	HalInitPL192();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
HAL_API void HalInitSystemMP()
{
	LogMsg("HalInitClock...");
	HalInitClock();
	LogMsg("HalInitTimer...");
	HalInitTimer();
	LogMsg("HalInitSystemMP done...");
}

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
	.GetMaximumInterruptCount = HalGetMaximumInterruptCount,
	.OnUpdateIpl = HalOnUpdateIpl,
	.VicRegisterInterrupt = HalVicRegisterInterrupt,
	.VicDeregisterInterrupt = HalVicDeregisterInterrupt,
	.OnInterruptRequest = HalOnInterruptRequest,
	.OnFastInterruptRequest = HalOnFastInterruptRequest,
	.Flags = HAL_VFTABLE_LOADED,
};

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	// Note! The HAL's driver object is kind of useless, it doesn't do anything actually.
	
	// Hook the HAL's functions.
	HalSetVftable(&HalpVfTable);
	
	// And return, that's all we need.
	return STATUS_SUCCESS;
}
