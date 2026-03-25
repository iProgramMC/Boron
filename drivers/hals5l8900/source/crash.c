/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/crash.c
	
Abstract:
	This module contains the iPod1,1 platform's specific crash routine.
	
Author:
	iProgramInCpp - 15 March 2026
***/
#include <ke.h>
#include "hali.h"

HAL_API
void HalProcessorCrashed()
{
	KeStopCurrentCPU();
}

NO_RETURN
void HalCrashSystem(const char* Message)
{
	DISABLE_INTERRUPTS();
	KeCrashConclusion(Message);
}
