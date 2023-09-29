/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/tsc.h
	
Abstract:
	This header file implements the interface for the
	Time Stamp Counter.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#ifndef BORON_HAL_IAPC_TSC_H
#define BORON_HAL_IAPC_TSC_H

uint64_t HalReadTsc();

#endif//BORON_HAL_IAPC_TSC_H