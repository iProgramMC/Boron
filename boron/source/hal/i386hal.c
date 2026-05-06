/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/i386hals.c
	
Abstract:
	This module contains the implementation of the HAL dispatch
	functions for the i386 architecture.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#include <ke.h>
#include <hal.h>

extern HAL_VFTABLE HalpVftable;

#ifdef TARGET_I386

void HalPicRegisterInterrupt(uint8_t Vector, KIPL Ipl)
{
	HalpVftable.PicRegisterInterrupt(Vector, Ipl);
}

void HalPicDeregisterInterrupt(uint8_t Vector, KIPL Ipl)
{
	HalpVftable.PicDeregisterInterrupt(Vector, Ipl);
}

#endif // TARGET_I386
