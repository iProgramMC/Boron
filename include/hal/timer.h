/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/timer.h
	
Abstract:
	This header file contains the definitions for the API
	for the generic system timer (GST) implemented by the
	HAL.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#ifndef BORON_HAL_TIMER_H
#define BORON_HAL_TIMER_H

uint64_t HalGetTicksPerSecond();

uint64_t HalGetTickCount();

void HalInitSystemTimer();

bool HalUseOneShotTimer();

void HalRequestInterruptInTicks(uint64_t ticks);

uint64_t HalGetInterruptDeltaTime();

#endif//BORON_HAL_TIMER_H
