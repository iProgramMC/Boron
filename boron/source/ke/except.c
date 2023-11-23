/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/except.c
	
Abstract:
	This module implements the kernel side exception
	handlers.
	
Author:
	iProgramInCpp - 13 October 2023
***/

#include "ki.h"
#include <mm.h>

void KeOnUnknownInterrupt(uintptr_t FaultPC, uintptr_t Vector)
{
#ifdef TARGET_AMD64
#define SPECIFIER "%02x"
#else
#define SPECIFIER "%08x"
#endif
	DbgPrint("** Unknown interrupt " SPECIFIER " at %p on CPU %u", Vector, FaultPC, KeGetCurrentPRCB()->LapicId);
	KeCrash("Unknown interrupt " SPECIFIER " at %p on CPU %u", Vector, FaultPC, KeGetCurrentPRCB()->LapicId);
#undef SPECIFIER
}

void KeOnDoubleFault(uintptr_t FaultPC)
{
	KeCrash("Double fault at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
}

void KeOnProtectionFault(uintptr_t FaultPC)
{
	KeCrash("General Protection Fault at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
}

void KeOnPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode)
{
#ifdef DEBUG2
	DbgPrint("handling fault ip=%p, faultaddr=%p, faultmode=%p", FaultPC, FaultAddress, FaultMode);
#endif
	
	int FaultReason = MmPageFault(FaultPC, FaultAddress, FaultMode);
	
	if (FaultReason == FAULT_HANDLED)
		return;
	
	KeCrash("unhandled fault ip=%p, faultaddr=%p, faultmode=%p, reason=%d", FaultPC, FaultAddress, FaultMode, FaultReason);
}
