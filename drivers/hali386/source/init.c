/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ha/init.c
	
Abstract:
	This module contains the two initialization functions
	(UP-init and MP-init)
	
Author:
	iProgramInCpp - 16 October 2025
***/
#include <ke.h>
#include "hali.h"
#include "pci.h"

static const HAL_VFTABLE HalpVfTable =
{
	.EndOfInterrupt = HalEndOfInterrupt,
	.RequestInterruptInTicks = HalRequestInterruptInTicks,
	.RequestIpi = HalRequestIpi,
	.InitSystemUP = HalInitSystemUP,
	.InitSystemMP = HalInitSystemMP,
	.DisplayString = HalDisplayString,
	.CrashSystem = HalCrashSystem,
	.ProcessorCrashed = HalProcessorCrashed,
	.UseOneShotIntTimer = HalUseOneShotIntTimer,
	.GetIntTimerFrequency = HalGetIntTimerFrequency,
	.GetTickCount = HalGetTickCount,
	.GetTickFrequency = HalGetTickFrequency,
	.GetIntTimerDeltaTicks = HalGetIntTimerDeltaTicks,
	.PicRegisterInterrupt = HalPicRegisterInterrupt,
	.PicDeregisterInterrupt = HalPicDeregisterInterrupt,
	.PciEnumerate = HalPciEnumerate,
	.PciConfigReadDword = HalPciConfigReadDword,
	.PciConfigReadWord = HalPciConfigReadWord,
	.PciConfigWriteDword = HalPciConfigWriteDword,
	.PciReadDeviceIdentifier = HalPciReadDeviceIdentifier,
	.PciReadBar = HalPciReadBar,
	.PciReadBarAddress = HalPciReadBarAddress,
	.PciReadBarIoAddress = HalPciReadBarIoAddress,
	.Flags = HAL_VFTABLE_LOADED,
};

// Initialize the HAL on the BSP, for all processors.
HAL_API void HalInitSystemUP()
{
	HalInitPic();
	HalInitTerminal();
}

// Initialize the HAL separately for each processor.
// This function is run on ALL processors.
HAL_API void HalInitSystemMP()
{
	HalInitTimer();
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	// Note! The HAL's driver object is kind of useless, it doesn't do anything actually.
	
	// Hook the HAL's functions.
	HalSetVftable(&HalpVfTable);
	
	// And return, that's all we need.
	return STATUS_SUCCESS;
}