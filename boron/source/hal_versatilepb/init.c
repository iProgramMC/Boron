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
#ifdef TARGET_ARMV6
#define IS_HAL
#include <ke.h>
#include <io.h>
#include "hali.h"

void HalVpbEndOfInterrupt(int InterruptNumber);
void HalVpbRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void HalVpbInitSystemUP();
void HalVpbInitSystemMP();
void HalVpbDisplayString(const char* Message);
void HalVpbCrashSystem(const char* Message) NO_RETURN;
bool HalVpbUseOneShotIntTimer();
void HalVpbProcessorCrashed() NO_RETURN;
uint64_t HalVpbGetIntTimerFrequency();
uint64_t HalVpbGetTickCount();
uint64_t HalVpbGetTickFrequency();
uint64_t HalVpbGetIntTimerDeltaTicks();
int HalVpbGetMaximumInterruptCount();
void HalVpbRegisterInterruptHandler(int Irq, void(*Func)());
PKREGISTERS HalVpbOnInterruptRequest(PKREGISTERS Registers);
PKREGISTERS HalVpbOnFastInterruptRequest(PKREGISTERS Registers);
bool HalVpbUseOneShotTimer();
void HalVpbRequestInterruptInTicks(uint64_t ticks);
uint64_t HalVpbGetInterruptDeltaTime();
void HalInitPL190();
void HalInitTimer();

// Initialize the HAL on the BSP, for all processors.
HAL_API void HalVpbInitSystemUP()
{
	HalInitPL190();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
HAL_API void HalVpbInitSystemMP()
{
	HalInitTimer();
}

void HalVpbDisplayString(const char* Message)
{
	//DbgPrint("%s(%s)", __func__, Message);
	DbgPrintString(Message);
}

static const HAL_VFTABLE HalpVfTable =
{
	.EndOfInterrupt = HalVpbEndOfInterrupt,
	.RequestInterruptInTicks = HalVpbRequestInterruptInTicks,
	.RequestIpi = HalVpbRequestIpi,
	.InitSystemUP = HalVpbInitSystemUP,
	.InitSystemMP = HalVpbInitSystemMP,
	.DisplayString = HalVpbDisplayString,
	.CrashSystem = HalVpbCrashSystem,
	.ProcessorCrashed = HalVpbProcessorCrashed,
	.UseOneShotIntTimer = HalVpbUseOneShotIntTimer,
	.GetIntTimerFrequency = HalVpbGetIntTimerFrequency,
	.GetTickCount = HalVpbGetTickCount,
	.GetTickFrequency = HalVpbGetTickFrequency,
	.GetIntTimerDeltaTicks = HalVpbGetIntTimerDeltaTicks,
	.GetMaximumInterruptCount = HalVpbGetMaximumInterruptCount,
	.RegisterInterruptHandler = HalVpbRegisterInterruptHandler,
	.OnInterruptRequest = HalVpbOnInterruptRequest,
	.OnFastInterruptRequest = HalVpbOnFastInterruptRequest,
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
