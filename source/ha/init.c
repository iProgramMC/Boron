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
#include "acpi.h"

void KiSetupIdt();

// Initialize the HAL on the BSP, for all processors.
void HalUPInit()
{
	// Initialize the IDT
	KiSetupIdt();
	HalInitAcpi();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
void HalMPInit()
{
	HalEnableApic();
}
