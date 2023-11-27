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

#ifdef TARGET_AMD64
#define KI_EXCEPTION_HANDLER_INIT() \
	UNUSED uint64_t FaultPC = TrapFrame->rip;      \
	UNUSED uint64_t FaultAddress = TrapFrame->cr2; \
	UNUSED uint64_t FaultMode    = TrapFrame->ErrorCode; \
	UNUSED int Vector = TrapFrame->IntNumber
#else
#error Go implement KI_EXCEPTION_HANDLER_INIT!
#endif

void KeOnUnknownInterrupt(PKREGISTERS TrapFrame)
{
	KI_EXCEPTION_HANDLER_INIT();
	
#ifdef TARGET_AMD64
#define SPECIFIER "%02x"
#else
#define SPECIFIER "%08x"
#endif
	DbgPrint("** Unknown interrupt " SPECIFIER " at %p on CPU %u", Vector, FaultPC, KeGetCurrentPRCB()->LapicId);
	KeCrash("Unknown interrupt " SPECIFIER " at %p on CPU %u", Vector, FaultPC, KeGetCurrentPRCB()->LapicId);
#undef SPECIFIER
}

void KeOnDoubleFault(PKREGISTERS TrapFrame)
{
	KI_EXCEPTION_HANDLER_INIT();
	KeCrash("Double fault at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
}

void KeOnProtectionFault(PKREGISTERS TrapFrame)
{
	KI_EXCEPTION_HANDLER_INIT();
	KeCrash("General Protection Fault at %p on CPU %u", FaultPC, KeGetCurrentPRCB()->LapicId);
}

extern void MmProbeAddressSubEarlyReturn();

void KeOnPageFault(PKREGISTERS TrapFrame)
{
	KI_EXCEPTION_HANDLER_INIT();
	
#ifdef DEBUG2
	DbgPrint("handling fault ip=%p, faultaddr=%p, faultmode=%p", FaultPC, FaultAddress, FaultMode);
#endif
	
	int FaultReason = MmPageFault(FaultPC, FaultAddress, FaultMode);
	
	if (FaultReason == FAULT_HANDLED)
		return;
	
	// Invalid page fault!
	PKTHREAD Thread = KeGetCurrentThread();
	
	// Check if we are probing.
	if (Thread && Thread->Probing)
	{
		// Yeah, so instead of crashing, just modify the trap frame to point to the
		// return instruction of MmProbeAddressSub, and RAX to return STATUS_FAULT.
	#ifdef TARGET_AMD64
		
		TrapFrame->rip = (uint64_t) MmProbeAddressSubEarlyReturn;
		TrapFrame->rax = (uint64_t) STATUS_FAULT;
		
		return;
		
	#else
		
		#error Hey!
		
	#endif
	}
	
	KeCrash("unhandled fault thread=%p, ip=%p, faultaddr=%p, faultmode=%p, reason=%d", Thread, FaultPC, FaultAddress, FaultMode, FaultReason);
}
