/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/crash.c
	
Abstract:
	This module contains the AMD64 platform's specific
	crash routine.
	
Author:
	iProgramInCpp - 16 September 2023
***/

#include "hali.h"
#include <ke.h>
#include <string.h>
#include "apic.h"

static KSPIN_LOCK HalpCrashLock;
static int        HalpCrashedProcessors;

void HalProcessorCrashed()
{
	AtAddFetch(HalpCrashedProcessors, 1);
	KeStopCurrentCPU();
}

void HalCrashSystem(const char* Message)
{
	KIPL Unused;
	// lock the crash in so that no one else can crash but us
	KeAcquireSpinLock(&HalpCrashLock, &Unused);
	AtClear(HalpCrashedProcessors);
	AtAddFetch(HalpCrashedProcessors, 1);
	
	// disable interrupts
	DISABLE_INTERRUPTS();
	
	int ProcessorCount = KeGetProcessorCount();
	
	// pre-lock the print and debug print lock so there are no threading issues later from
	// stopping the CPUs.
	// TODO: Also lock other important used locks too
	//KeAcquireSpinLock(&g_PrintLock);
	//KeAcquireSpinLock(&g_DebugPrintLock);
	
	// Send IPI to the other processors to make them halt.
	// Don't send an IPI to ourselves as we already are in the process of crashing.
	HalRequestIpi(0, HAL_IPI_BROADCAST, KiVectorCrash);
	
	//HalPrintStringDebug("CRASH: Waiting for all processors to halt!\n");
	while (AtLoad(HalpCrashedProcessors) != ProcessorCount)
		KeSpinningHint();
	
	KeCrashConclusion(Message);
}
