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
#include <ke.h>
#include "hali.h"

NO_RETURN
void HalRaspi1apCrashSystem(const char* Message)
{
	DbgPrint("TODO %s", __func__);
	while (1) {
		ASM("wfi":::"memory");
	}
}

NO_RETURN
void HalRaspi1apProcessorCrashed()
{
	DbgPrint("TODO %s", __func__);
	while (1) {
		ASM("wfi":::"memory");
	}
}

int HalRaspi1apGetMaximumInterruptCount()
{
	DbgPrint("TODO %s", __func__);
	return 1;
}
