/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/__troll.c
	
Abstract:
	This module contains the troll.
	
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

void HalInitializeTemporary()
{
	HalpVftable.EndOfInterrupt = _HalEndOfInterrupt;
	HalpVftable.RequestInterruptInTicks = _HalRequestInterruptInTicks;
	HalpVftable.RequestIpi = _HalRequestIpi;
	HalpVftable.InitSystemUP = _HalInitSystemUP;
	HalpVftable.InitSystemMP = _HalInitSystemMP;
	HalpVftable.DisplayString = _HalDisplayString;
	HalpVftable.CrashSystem = _HalCrashSystem;
	HalpVftable.ProcessorCrashed = _HalProcessorCrashed;
	HalpVftable.UseOneShotIntTimer = _HalUseOneShotIntTimer;
	HalpVftable.GetIntTimerFrequency = _HalGetIntTimerFrequency;
	HalpVftable.GetTickCount = _HalGetTickCount;
	HalpVftable.GetTickFrequency = _HalGetTickFrequency;
	HalpVftable.Flags |= HAL_VFTABLE_LOADED;
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

NO_RETURN void HalProcessorCrashed()
{
	HalpVftable.ProcessorCrashed();
	KeStopCurrentCPU();
}
