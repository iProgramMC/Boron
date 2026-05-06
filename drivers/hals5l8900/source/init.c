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
#include "pl192.h"
#include "clock.h"
#include "timer.h"
#include "gpio.h"
#include "crash.h"

void HalEndOfInterrupt(int InterruptNumber);
void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector);
void HalInitSystemUP();
void HalInitSystemMP();
void HalDisplayString(const char* Message);
void HalInitCLCD();
void HalInitTerminal();

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
	HalInitClock();
	HalInitTimer();
	HalInitGpio();
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
	.SetEnabledClockGate = HalSetEnabledClockGate,
	.RegisterGpioInterrupt = HalRegisterGpioInterrupt,
	.GetPinStateGpio = HalGetPinStateGpio,
	.FselGpio = HalFselGpio,
	.SetInputPinGpio = HalSetInputPinGpio,
	.SetOutputPinGpio = HalSetOutputPinGpio,
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
