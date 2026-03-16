/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/timer.c
	
Abstract:
	This module contains the platform's timer driver.
	
Author:
	iProgramInCpp - 15 March 2026
***/
#include "hali.h"

uint64_t HalGetIntTimerFrequency()
{
	DbgPrint("%s NYI", __func__);
	return 1000000;
}

uint64_t HalGetIntTimerDeltaTicks()
{
	DbgPrint("%s NYI", __func__);
	return 1000;
}

uint64_t HalGetInterruptDeltaTime()
{
	DbgPrint("%s NYI", __func__);
	return 1000;
}

bool HalUseOneShotIntTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}

void HalRequestInterruptInTicks(uint64_t ticks)
{
	DbgPrint("%s NYI", __func__);
}

void HalInitTimer()
{
	DbgPrint("%s NYI", __func__);
	
	
	// Also initialize the clock
	HalInitClock();
}
