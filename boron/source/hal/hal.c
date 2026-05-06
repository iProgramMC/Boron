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

// i386: keep interrupts enabled, else the timer may be unreliable and may just
// return the same value even if significantly more time has passed.
#if defined DEBUG && !defined CONFIG_SMP && !defined CONFIG_I386
#define TICK_DEBUG
#endif

HAL_VFTABLE HalpVftable;

bool HalWasInitted()
{
	return HalpVftable.Flags & HAL_VFTABLE_LOADED;
}

void HalSetVftable(const HAL_VFTABLE* Table)
{
	HalpVftable = *Table;
}

void HalEndOfInterrupt(int InterruptNumber)
{
	HalpVftable.EndOfInterrupt(InterruptNumber);
}

void HalRequestInterruptInTicks(uint64_t Ticks)
{
	HalpVftable.RequestInterruptInTicks(Ticks);
}

void HalRequestIpi(uint32_t LapicId, uint32_t Flags, int Vector)
{
	HalpVftable.RequestIpi(LapicId, Flags, Vector);
}

INIT
void HalInitSystemUP()
{
	HalpVftable.InitSystemUP();
}

INIT
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
#ifdef TICK_DEBUG
	static uint64_t LastTickCount = 0;
	
	bool Restore = KeDisableInterrupts();
#endif

	uint64_t TickCount = HalpVftable.GetTickCount();

#ifdef TICK_DEBUG
	if (LastTickCount > TickCount)
	{
		KeCrash(
			"ERROR: LastTickCount: %lld,  TickCount: %lld. Timer went backwards?",
			LastTickCount,
			TickCount
		);
	}
	
	LastTickCount = TickCount;
	KeRestoreInterrupts(Restore);
#endif
	
	return TickCount;
}

uint64_t HalGetTickFrequency()
{
	return HalpVftable.GetTickFrequency();
}

uint64_t HalGetIntTimerDeltaTicks()
{
	return HalpVftable.GetIntTimerDeltaTicks();
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

void HalBeginShutdown()
{
	if (HalpVftable.BeginShutdown)
		HalpVftable.BeginShutdown();
}

NO_RETURN
void HalPerformPoweroff(bool Reboot)
{
	if (!HalpVftable.PerformPoweroff)
	{
		KeCrash(
			"Perform power off%s",
			Reboot ? " (User requested a reboot)" : ""
		);
	}
	
	HalpVftable.PerformPoweroff(Reboot);
}
