/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/pic.c
	
Abstract:
	This module contains support routines for the PIC.
	
Author:
	iProgramInCpp - 16 October 2025
***/
#include <ke.h>
#include "hali.h"

HAL_API void HalRequestIpi(UNUSED uint32_t HardwareId, UNUSED uint32_t Flags, UNUSED int Vector)
{
	DbgPrint("HalRequestIpi not implemented");
}

void HalEndOfInterrupt(int InterruptNumber)
{
	if (InterruptNumber >= 0x28)
		KePortWriteByte(PIC_SUB_COMMAND, PIC_CMD_EOI);
	
	KePortWriteByte(PIC_MAIN_COMMAND, PIC_CMD_EOI);
}
