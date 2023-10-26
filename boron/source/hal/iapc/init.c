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

void KiSetupIdt();

PKHALCB KeGetCurrentHalCB()
{
	return KeGetCurrentPRCB()->HalData;
}

// Initialize the HAL on the BSP, for all processors.
void HalUPInit()
{
	// Initialize the IDT
	KiSetupIdt();
	HalInitAcpi();
	HalInitIoApic();
	HpetInitialize();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
void HalMPInit()
{
	KeGetCurrentPRCB()->HalData = ExAllocateSmall(sizeof(KHALCB));
	
	HalEnableApic();
	HalCalibrateApic();
}

void _HalInitSystemUP()
{
	HalUPInit();
}

void _HalInitSystemMP()
{
	HalMPInit();
}
