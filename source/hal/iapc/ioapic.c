/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/ioapic.c
	
Abstract:
	This module contains the implementation of the IOAPIC driver.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include <ke.h>
#include "acpi.h"
#include "apic.h"

extern PSDT_HEADER HalpSdtApic;

void HalInitIoApic()
{
	if (!HalIsApicAvailable())
	{
		KeCrashBeforeSMPInit("The APIC is not available. How?!");
	}
	
	// @TODO
}
