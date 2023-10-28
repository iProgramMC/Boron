/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/fault.c
	
Abstract:
	This module implements the generic timer API on
	top of the APIC driver.
	
Author:
	iProgramInCpp - 27 September 2023
***/
#include <hal.h>
#include <ke.h>
#include "apic.h"
#include "tsc.h"

/***
	Function description:
		Obtains the frequency of the system timer.
		
		Note: This could be different across CPUs.
	
	Parameters:
		None.
	
	Return value:
		The frequency of the generic system timer.
		
	Notes:
		This can vary from CPU to CPU.
***/
uint64_t _HalGetTickFrequency()
{
	return KeGetCurrentHalCB()->TscFrequency;
}

/***
	Function description:
		Returns the number of milliseconds that have
		passed since system initialization.
	
	Return value:
		Ditto.
	
	Parameters:
		None.
	
	Notes:
		* Don't assume that a time of '0' means anything.
		* This can vary from CPU to CPU. Don't assume that
		  the frequency is the same across all processors.
***/
uint64_t _HalGetTickCount()
{
	return HalReadTsc();
}

/***
	Function description:
		See return value.
	
	Parameters:
		None.
	
	Return value:
		Returns whether the generic system timer is in oneshot (true)
		or periodic (zero).  If yes, the caller can call the function
		HalRequestInterruptInTicks().
***/
bool _HalUseOneShotIntTimer()
{
	// The APIC timer is supposed to be available on the IA-PC 64 platform.
	return true;
}

/***
	Function description:
		Obtains the frequency of the system interrupt timer.
	
	Parameters:
		None.
	
	Return value:
		The frequency of the generic system interrupt timer.
		
	Notes:
		This can vary from CPU to CPU.
***/
uint64_t _HalGetIntTimerFrequency()
{
	return KeGetCurrentHalCB()->LapicFrequency;
}

/***
	Function description:
		Requests an interrupt from the generic system timer, in the
		specified number of IT ticks. Only call if _HalUseOneShotIntTimer
		returns true. Otherwise, behavior is undefined.
	
	Parameters:
		None.
	
	Return value:
		None.
***/
void _HalRequestInterruptInTicks(uint64_t ticks)
{
#ifdef DEBUG
	if (!_HalUseOneShotIntTimer())
		KeCrash("Hey, you can't use HalRequestInterruptInTicks");
#endif
	
	HalApicSetIrqIn(ticks);
	
	return;
}

/***
	Function description:
		Returns the delta time between interrupts in IT ticks.
		Do not use if _HalUseOneShotIntTimer() returns true.
	
	Parameters:
		None.
	
	Return value:
		The delta time between interrupts in ticks.
***/
uint64_t HalGetInterruptDeltaTime()
{
#ifdef DEBUG
	if (_HalUseOneShotIntTimer())
		KeCrash("Hey, you can't use HalGetInterruptDeltaTime");
#endif
	
	// @TODO
	
	return 0;
}
