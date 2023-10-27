/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/init.c
	
Abstract:
	This module contains the two initialization functions
	(UP-init and MP-init)
	
Author:
	iProgramInCpp - 23 September 2023
***/
#include <hal.h>
#include <ke.h>
#include <ex.h>
#include "acpi.h"
#include "apic.h"
#include "hpet.h"

PKHALCB KeGetCurrentHalCB()
{
	return KeGetCurrentPRCB()->HalData;
}

// Initialize the HAL on the BSP, for all processors.
void _HalInitSystemUP()
{
	HalInitTerminal();
	LogMsg("Boron (TM), October 2023 - V0.005");
	
	HalInitAcpi();
	HalInitIoApic();
	HpetInitialize();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
void _HalInitSystemMP()
{
	KeGetCurrentPRCB()->HalData = ExAllocateSmall(sizeof(KHALCB));
	
	HalEnableApic();
	HalCalibrateApic();
}
