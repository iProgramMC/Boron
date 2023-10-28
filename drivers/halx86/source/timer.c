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
#include "hali.h"
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
HAL_API uint64_t HalGetTickFrequency()
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
HAL_API uint64_t HalGetTickCount()
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
HAL_API bool HalUseOneShotIntTimer()
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
HAL_API uint64_t HalGetIntTimerFrequency()
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
HAL_API void HalRequestInterruptInTicks(uint64_t ticks)
{
#ifdef DEBUG
	if (!HalUseOneShotIntTimer())
		KeCrash("Hey, you can't use HalRequestInterruptInTicks");
#endif
	
	HalApicSetIrqIn(ticks);
	
	return;
}

/***
	Function description:
		Returns the delta time between interrupts in IT ticks.
		Do not use if HalUseOneShotIntTimer() returns true.
	
	Parameters:
		None.
	
	Return value:
		The delta time between interrupts in ticks.
***/
HAL_API uint64_t HalGetIntTimerDeltaTicks()
{
#ifdef DEBUG
	if (HalUseOneShotIntTimer())
		KeCrash("Hey, you can't use HalGetIntTimerDeltaTicks");
#endif
	
	// @TODO
	
	return 0;
}
