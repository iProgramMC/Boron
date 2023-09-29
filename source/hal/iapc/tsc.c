/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/tsc.h
	
Abstract:
	This module implements the interface for the Time
	Stamp Counter.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#include <main.h>
#include "tsc.h"

uint64_t HalReadTsc()
{
	uint64_t low, high;
	
	// note: The rdtsc instruction is specified to zero out the top 32 bits of rax and rdx.
	ASM("rdtsc":"=a"(low), "=d"(high));
	
	// So something like this is fine.
	return high << 32 | low;
}