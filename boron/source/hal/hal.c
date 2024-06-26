/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	hal/hal.c
	
Abstract:
	This module contains the implementation of
	the HAL dispatch functions.
	
Author:
	iProgramInCpp - 26 October 2023
***/
#include <hal.h>
#include <ke.h>

HAL_VFTABLE HalpVftable;

bool HalWasInitted()
{
	return HalpVftable.Flags & HAL_VFTABLE_LOADED;
}

void HalSetVftable(PHAL_VFTABLE Table)
{
	HalpVftable = *Table;
}

void HalEndOfInterrupt()
{
	HalpVftable.EndOfInterrupt();
}

void HalRequestInterruptInTicks(uint64_t Ticks)
{
	HalpVftable.RequestInterruptInTicks(Ticks);
}

void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	HalpVftable.RequestIpi(LapicId, Flags, Vector);
}

void HalInitSystemUP()
{
	HalpVftable.InitSystemUP();
}

void HalInitSystemMP()
{
	HalpVftable.InitSystemMP();
}

void HalDisplayString(const char* Message)
{
	if (!HalWasInitted())
	{
	#ifdef DEBUG
		DbgPrintString(Message);
	#endif
		return;
	}
	
	HalpVftable.DisplayString(Message);
}

NO_RETURN void HalCrashSystem(const char* Message)
{
	HalpVftable.CrashSystem(Message);
}

bool HalUseOneShotIntTimer()
{
	return HalpVftable.UseOneShotIntTimer();
}

uint64_t HalGetIntTimerFrequency()
{
	return HalpVftable.GetIntTimerFrequency();
}

uint64_t HalGetTickCount()
{
	return HalpVftable.GetTickCount();
}

uint64_t HalGetTickFrequency()
{
	return HalpVftable.GetTickFrequency();
}

uint64_t HalGetIntTimerDeltaTicks()
{
	return HalpVftable.GetIntTimerDeltaTicks();
}

void HalIoApicSetIrqRedirect(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status)
{
	return HalpVftable.IoApicSetIrqRedirect(Vector, Irq, LapicId, Status);
}

NO_RETURN void HalProcessorCrashed()
{
	HalpVftable.ProcessorCrashed();
	KeStopCurrentCPU();
}

PKREGISTERS KiHandleCrashIpi(PKREGISTERS Regs)
{
	DISABLE_INTERRUPTS();
	
	HalProcessorCrashed();
	KeStopCurrentCPU();
	return Regs;
}
