/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/pit.h
	
Abstract:
	This header file implements the interface for the
	PIT device. Used to calibrate the LAPIC timer.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#ifndef BORON_HAL_IAPC_PIT_H
#define BORON_HAL_IAPC_PIT_H

uint16_t HalReadPit();
void HalWritePit(uint16_t Data);

#endif//BORON_HAL_IAPC_PIT_H
