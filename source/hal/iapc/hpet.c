/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	hal/iapc/hpet.c
	
Abstract:
	This module contains the implementation of the HPET
	timer driver for the IA-PC HAL.
	
Author:
	iProgramInCpp - 29 September 2023
***/
#include <hal.h>
#include <mm.h>
#include "acpi.h"
#include "hpet.h"

static bool HpetpIsAvailable = false;

extern PRSDT_TABLE HalSdtHpet;

bool HpetIsAvailable()
{
	return HpetpIsAvailable;
}

void HpetInitialize()
{
	if (!HalSdtHpet)
	{
		SLogMsg("HpetInitialize: There is no HPET installed.");
	}
	
	// Map the HPET as uncacheable. 
}
