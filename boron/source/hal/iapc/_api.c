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

void _HalEndOfInterrupt();
void _HalRequestInterruptInTicks(uint64_t Ticks);
void _HalRequestIpi(uint32_t LapicId, uint32_t Flags);
void _HalInitSystemUP();
void _HalInitSystemMP();
void _HalDisplayString(const char* Message);
void _HalCrashSystem(const char* Message) NO_RETURN;
bool _HalUseOneShotIntTimer();
void _HalProcessorCrashed() NO_RETURN;
uint64_t _HalGetIntTimerFrequency();
uint64_t _HalGetTickCount();
uint64_t _HalGetTickFrequency();

void HalEndOfInterrupt()
{
	_HalEndOfInterrupt();
}

void HalRequestInterruptInTicks(uint64_t Ticks)
{
	_HalRequestInterruptInTicks(Ticks);
}

void HalRequestIpi(uint32_t LapicId, uint32_t Flags)
{
	_HalRequestIpi(LapicId, Flags);
}

void HalInitSystemUP()
{
	_HalInitSystemUP();
}

void HalInitSystemMP()
{
	_HalInitSystemMP();
}

void HalDisplayString(const char* Message)
{
	_HalDisplayString(Message);
}

NO_RETURN void HalCrashSystem(const char* Message)
{
	_HalCrashSystem(Message);
}

bool HalUseOneShotIntTimer()
{
	return _HalUseOneShotIntTimer();
}

uint64_t HalGetIntTimerFrequency()
{
	return _HalGetIntTimerFrequency();
}

uint64_t HalGetTickCount()
{
	return _HalGetTickCount();
}

uint64_t HalGetTickFrequency()
{
	return _HalGetTickFrequency();
}

NO_RETURN void HalProcessorCrashed()
{
	_HalProcessorCrashed();
	KeStopCurrentCPU();
}
