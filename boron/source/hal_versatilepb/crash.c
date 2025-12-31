/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/timer.c
	
Abstract:
	This module implements the HAL-specific portions of the
	crash process.
	
Author:
	iProgramInCpp - 31 December 2025
***/
#ifdef TARGET_ARMV6
#include <ke.h>
#include "hali.h"

NO_RETURN
void HalVpbCrashSystem(const char* Message)
{
	DISABLE_INTERRUPTS();
	KeCrashConclusion(Message);
}

NO_RETURN
void HalVpbProcessorCrashed()
{
	KeStopCurrentCPU();
}

#endif
