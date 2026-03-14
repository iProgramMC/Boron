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
#ifdef TARGET_ARM
#define IS_HAL
#include <ke.h>
#include <io.h>
#include "hali.h"

void HalIpodEndOfInterrupt(int InterruptNumber);
void HalIpodRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void HalIpodInitSystemUP();
void HalIpodInitSystemMP();
void HalIpodDisplayString(const char* Message);
void HalIpodCrashSystem(const char* Message) NO_RETURN;
bool HalIpodUseOneShotIntTimer();
void HalIpodProcessorCrashed() NO_RETURN;
uint64_t HalIpodGetIntTimerFrequency();
uint64_t HalIpodGetTickCount();
uint64_t HalIpodGetTickFrequency();
uint64_t HalIpodGetIntTimerDeltaTicks();
int HalIpodGetMaximumInterruptCount();
void HalIpodRegisterInterruptHandler(int Irq, void(*Func)());
PKREGISTERS HalIpodOnInterruptRequest(PKREGISTERS Registers);
PKREGISTERS HalIpodOnFastInterruptRequest(PKREGISTERS Registers);
bool HalIpodUseOneShotTimer();
void HalIpodRequestInterruptInTicks(uint64_t ticks);
uint64_t HalIpodGetInterruptDeltaTime();
void HalIpodInitCLCD();
void HalIpodInitTerminal();
void HalInitPL192();
void HalInitTimer();

// Initialize the HAL on the BSP, for all processors.
HAL_API void HalIpodInitSystemUP()
{
	HalIpodInitCLCD();
	HalIpodInitTerminal();
	HalInitPL192();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
HAL_API void HalIpodInitSystemMP()
{
	HalInitTimer();
}

static const HAL_VFTABLE HalpVfTable =
{
	.EndOfInterrupt = HalIpodEndOfInterrupt,
	.RequestInterruptInTicks = HalIpodRequestInterruptInTicks,
	.RequestIpi = HalIpodRequestIpi,
	.InitSystemUP = HalIpodInitSystemUP,
	.InitSystemMP = HalIpodInitSystemMP,
	.DisplayString = HalIpodDisplayString,
	.CrashSystem = HalIpodCrashSystem,
	.ProcessorCrashed = HalIpodProcessorCrashed,
	.UseOneShotIntTimer = HalIpodUseOneShotIntTimer,
	.GetIntTimerFrequency = HalIpodGetIntTimerFrequency,
	.GetTickCount = HalIpodGetTickCount,
	.GetTickFrequency = HalIpodGetTickFrequency,
	.GetIntTimerDeltaTicks = HalIpodGetIntTimerDeltaTicks,
	.GetMaximumInterruptCount = HalIpodGetMaximumInterruptCount,
	.RegisterInterruptHandler = HalIpodRegisterInterruptHandler,
	.OnInterruptRequest = HalIpodOnInterruptRequest,
	.OnFastInterruptRequest = HalIpodOnFastInterruptRequest,
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

#endif