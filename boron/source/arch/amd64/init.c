/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/init.c
	
Abstract:
	This module implements the architecture specific UP-init
	and MP-init routines.
	
Author:
	iProgramInCpp - 27 October 2023
***/
#include <ke.h>
#include <hal.h>
#include "archi.h"

int KiVectorCrash,
    KiVectorTlbShootdown;

// Used by drivers.  The kernel uses the values directly.
int KeGetSystemInterruptVector(int Number)
{
	switch (Number)
	{
		case KGSIV_NONE:
		default:                  return 0;
		case KGSIV_CRASH:         return KiVectorCrash;
		case KGSIV_TLB_SHOOTDOWN: return KiVectorTlbShootdown;
	}
}

void KiSetupIdt();

void KeInitArchUP()
{
	KiSetupIdt();
	
	// Initialize interrupt vectors for certain things
	KiVectorCrash        = KeAllocateInterruptVector(IPL_NOINTS);
	KiVectorTlbShootdown = KeAllocateInterruptVector(IPL_NOINTS);
	
	KeRegisterInterrupt(KiVectorTlbShootdown, KiHandleTlbShootdownIpi);
	KeRegisterInterrupt(KiVectorCrash,        KiHandleCrashIpi);
	
	KiInitializeInterruptSystem();
}

void KeInitArchMP()
{
	KeInitCPU();
	KeLowerIPL(IPL_NORMAL);
	HalInitSystemMP();
}
