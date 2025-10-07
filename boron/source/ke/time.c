/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ke/version.c
	
Abstract:
	This module implements time related system services.
	
Author:
	iProgramInCpp - 6 October 2025
***/
#include <ke.h>
#include <mm.h>
#include <hal.h>

BSTATUS OSGetTickCount(uint64_t* TickCount)
{
	uint64_t Ticks = HalGetTickCount();
	return MmSafeCopy(TickCount, &Ticks, sizeof(uint64_t), KeGetPreviousMode(), true);
}

BSTATUS OSGetTickFrequency(uint64_t* TickFrequency)
{
	uint64_t Frequency = HalGetTickFrequency();
	return MmSafeCopy(TickFrequency, &Frequency, sizeof(uint64_t), KeGetPreviousMode(), true);
}
