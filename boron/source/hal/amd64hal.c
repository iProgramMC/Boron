/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	hal/amd64hal.c
	
Abstract:
	This module contains the implementation of the HAL dispatch
	functions for the AMD64 architecture.
	
Author:
	iProgramInCpp - 6 May 2026
***/
#include <ke.h>
#include <hal.h>

extern HAL_VFTABLE HalpVftable;

#ifdef TARGET_AMD64

void HalIoApicSetIrqRedirect(uint8_t Vector, uint8_t Irq, uint32_t LapicId, bool Status)
{
	return HalpVftable.IoApicSetIrqRedirect(Vector, Irq, LapicId, Status);
}

#endif // TARGET_AMD64
