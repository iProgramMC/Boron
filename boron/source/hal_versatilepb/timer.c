/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/timer.c
	
Abstract:
	This module contains the driver for the timer.
	
Author:
	iProgramInCpp - 31 December 2025
***/
#ifdef TARGET_ARMV6
#include <ke.h>
#include "hali.h"

bool HalVpbUseOneShotIntTimer()
{
	DbgPrint("TODO %s", __func__);
	return false;
}

bool HalVpbUseOneShotTimer()
{
	DbgPrint("TODO %s", __func__);
	return false;
}

uint64_t HalVpbGetIntTimerFrequency()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalVpbGetTickCount()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalVpbGetTickFrequency()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalVpbGetIntTimerDeltaTicks()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalVpbGetInterruptDeltaTime()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

void HalVpbRequestInterruptInTicks(uint64_t ticks)
{
	DbgPrint("TODO %s", __func__);
}

#endif
