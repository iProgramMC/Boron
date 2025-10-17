/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/crash.c
	
Abstract:
	This module contains the i386 platform's specific crash routine.
	
Author:
	iProgramInCpp - 17 October 2025
***/
#include <ke.h>
#include "hali.h"

HAL_API void HalProcessorCrashed()
{
	KeStopCurrentCPU();
}

void HalCrashSystem(const char* Message)
{
	DISABLE_INTERRUPTS();
	KeCrashConclusion(Message);
}
