/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/init.c
	
Abstract:
	This module contains the two initialization functions
	(UP-init and MP-init)
	
Author:
	iProgramInCpp - 31 December 2025
***/
#define IS_HAL
#include <ke.h>
#include <io.h>
#include "hali.h"

void HalRaspi1apEndOfInterrupt(int InterruptNumber);
void HalRaspi1apRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void HalRaspi1apInitSystemUP();
void HalRaspi1apInitSystemMP();
void HalRaspi1apDisplayString(const char* Message);
void HalRaspi1apCrashSystem(const char* Message) NO_RETURN;
bool HalRaspi1apUseOneShotIntTimer();
void HalRaspi1apProcessorCrashed() NO_RETURN;
uint64_t HalRaspi1apGetIntTimerFrequency();
uint64_t HalRaspi1apGetTickCount();
uint64_t HalRaspi1apGetTickFrequency();
uint64_t HalRaspi1apGetIntTimerDeltaTicks();
int HalRaspi1apGetMaximumInterruptCount();
void HalRaspi1apRegisterInterruptHandler(int Irq, void(*Func)());
PKREGISTERS HalRaspi1apOnInterruptRequest(PKREGISTERS Registers);
PKREGISTERS HalRaspi1apOnFastInterruptRequest(PKREGISTERS Registers);
uint64_t HalRaspi1apGetTickFrequency();
uint64_t HalRaspi1apGetTickCount();
uint64_t HalRaspi1apGetIntTimerFrequency();
bool HalRaspi1apUseOneShotTimer();
void HalRaspi1apRequestInterruptInTicks(uint64_t ticks);
uint64_t HalRaspi1apGetInterruptDeltaTime();
void HalInitPL190();
void HalInitTimer();

// Initialize the HAL on the BSP, for all processors.
HAL_API void HalRaspi1apInitSystemUP()
{
	HalInitPL190();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
HAL_API void HalRaspi1apInitSystemMP()
{
	HalInitTimer();
}

static const HAL_VFTABLE HalpVfTable =
{
	.EndOfInterrupt = HalRaspi1apEndOfInterrupt,
	.RequestInterruptInTicks = HalRaspi1apRequestInterruptInTicks,
	.RequestIpi = HalRaspi1apRequestIpi,
	.InitSystemUP = HalRaspi1apInitSystemUP,
	.InitSystemMP = HalRaspi1apInitSystemMP,
	.DisplayString = HalRaspi1apDisplayString,
	.CrashSystem = HalRaspi1apCrashSystem,
	.ProcessorCrashed = HalRaspi1apProcessorCrashed,
	.UseOneShotIntTimer = HalRaspi1apUseOneShotIntTimer,
	.GetIntTimerFrequency = HalRaspi1apGetIntTimerFrequency,
	.GetTickCount = HalRaspi1apGetTickCount,
	.GetTickFrequency = HalRaspi1apGetTickFrequency,
	.GetIntTimerDeltaTicks = HalRaspi1apGetIntTimerDeltaTicks,
	.GetMaximumInterruptCount = HalRaspi1apGetMaximumInterruptCount,
	.RegisterInterruptHandler = HalRaspi1apRegisterInterruptHandler,
	.OnInterruptRequest = HalRaspi1apOnInterruptRequest,
	.OnFastInterruptRequest = HalRaspi1apOnFastInterruptRequest,
	.Flags = HAL_VFTABLE_LOADED,
};

BSTATUS HAL_DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	// Note! The HAL's driver object is kind of useless, it doesn't do anything actually.
	
	// Hook the HAL's functions.
	HalSetVftable(&HalpVfTable);
	
	// And return, that's all we need.
	return STATUS_SUCCESS;
}
