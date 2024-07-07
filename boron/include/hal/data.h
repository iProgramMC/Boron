/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/timer.h
	
Abstract:
	This header file contains the definition for the processor
	specific HAL data structure.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#ifndef BORON_HAL_DATA_H
#define BORON_HAL_DATA_H

// HAL Control Block
typedef struct KHALCB_tag
{
	// LAPIC and TSC frequencies, in ticks/ms.
	uint64_t LapicFrequency;
	uint64_t TscFrequency;
}
KHALCB, *PKHALCB;

PKHALCB KeGetCurrentHalCB();

#endif//BORON_HAL_DATA_H
