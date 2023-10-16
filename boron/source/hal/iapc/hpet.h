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

typedef union HPET_GENERAL_CAPS_tag
{
	struct
	{
		unsigned RevID        : 8;
		unsigned NumTimCap    : 5;
		unsigned CountSizeCap : 1;
		unsigned Reserved     : 1;
		unsigned LegRouteCap  : 1;
		unsigned VendorID     : 16;
		unsigned CounterClockPeriod : 32;
	}
	PACKED;
	
	uint64_t Contents;
}
HPET_GENERAL_CAPS, *PHPET_GENERAL_CAPS;

typedef struct HPET_TIMER_INFO_tag
{
	uint64_t ConfigAndCaps;
	uint64_t ComparatorValue;
	uint64_t FSBInterruptRoute;
	uint64_t Reserved;
}
HPET_TIMER_INFO, *PHPET_TIMER_INFO;

typedef struct HPET_REGISTERS_tag
{
	uint64_t GeneralCaps;
	uint64_t Reserved8;
	uint64_t GeneralConfig;
	uint64_t Reserved18;
	uint64_t GeneralIrqStatus;
	uint64_t Reserved28;
	uint64_t ReservedArr[24];
	uint64_t CounterValue;
	uint64_t ReservedF8;
	HPET_TIMER_INFO Timers[32];
}
HPET_REGISTERS, *PHPET_REGISTERS;

void HpetInitialize();

bool HpetIsAvailable();

uint64_t HpetReadValue();

uint64_t HpetGetFrequency();

#endif//BORON_HAL_HPET_H
