/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/hpet.h
	
Abstract:
	This header file contains the definitions for the
	HPET device driver.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#ifndef BORON_HAL_HPET_H
#define BORON_HAL_HPET_H

void HpetInitialize();

bool HpetIsAvailable();

#endif//BORON_HAL_HPET_H
