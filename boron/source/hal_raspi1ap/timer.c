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
#include <ke.h>
#include "hali.h"

bool HalRaspi1apUseOneShotIntTimer()
{
	DbgPrint("TODO %s", __func__);
	return false;
}

bool HalRaspi1apUseOneShotTimer()
{
	DbgPrint("TODO %s", __func__);
	return false;
}

uint64_t HalRaspi1apGetIntTimerFrequency()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalRaspi1apGetTickCount()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalRaspi1apGetTickFrequency()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalRaspi1apGetIntTimerDeltaTicks()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

uint64_t HalRaspi1apGetInterruptDeltaTime()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}

void HalRaspi1apRequestInterruptInTicks(uint64_t ticks)
{
	DbgPrint("TODO %s", __func__);
}
