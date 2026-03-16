/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ha/clock.c
	
Abstract:
	This module contains the platform's clock driver.
	
Author:
	iProgramInCpp - 16 March 2026
***/
#include "hali.h"

uint64_t HalGetTickCount()
{
	DbgPrint("%s NYI", __func__);
	return 1;
}

uint64_t HalGetTickFrequency()
{
	DbgPrint("%s NYI", __func__);
	return 1000000;
}

bool HalUseOneShotTimer()
{
	DbgPrint("%s NYI", __func__);
	return false;
}

void HalInitClock()
{
	
}
