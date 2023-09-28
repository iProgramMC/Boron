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
#include "apic.h"

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
uint64_t HalGetTicksPerSecond()
{
	// @TODO
	return 0;
}

/***
	Function description:
		Returns the number of system timer ticks that have
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
uint64_t HalGetTickCount()
{
	// @TODO
	return 0;
}

/***
	Function description:
		Initializes the generic system timer.
		On AMD64, this is implemented using the APIC, and calibrated
		using the following:
		* The CPUID base frequency leaf (most accurate)
		* The HPET (great accuracy)
		* The PIT  (worst accuracy)
	
	Parameters:
		None.
	
	Return value:
		None.
***/
void HalInitSystemTimer()
{
	
}
