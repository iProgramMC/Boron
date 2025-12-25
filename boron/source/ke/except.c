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

#if defined TARGET_AMD64

#define KI_EXCEPTION_HANDLER_INIT() \
	UNUSED uint64_t FaultPC = TrapFrame->rip;      \
	UNUSED uint64_t FaultAddress = TrapFrame->cr2; \
	UNUSED uint64_t FaultMode    = TrapFrame->ErrorCode; \
	UNUSED int Vector = (int)TrapFrame->IntNumber

#elif defined TARGET_I386

#define KI_EXCEPTION_HANDLER_INIT() \
	UNUSED uint32_t FaultPC = TrapFrame->Eip;      \
	UNUSED uint32_t FaultAddress = TrapFrame->Cr2; \
	UNUSED uint64_t FaultMode    = TrapFrame->ErrorCode; \
	UNUSED int Vector = TrapFrame->IntNumber

#elif defined TARGET_ARM

// TODO: an actual vector number
#define KI_EXCEPTION_HANDLER_INIT() \
	UNUSED uint32_t FaultPC = TrapFrame->Pc; \
	UNUSED uint32_t FaultAddress, FaultMode; \
	UNUSED uint32_t Vector = TrapFrame->IntNumber; \
	if (KiWasFaultCausedByInstructionFetch()) { \
		FaultAddress = KiReadIfar(); \
		FaultMode = KiReadIfsr(); \
	} else { \
		FaultAddress = KiReadDfar(); \
		FaultMode = KiReadDfsr(); \
	}

#else

#error Go implement KI_EXCEPTION_HANDLER_INIT!

#endif

void KeOnUnknownInterrupt(PKREGISTERS TrapFrame)
{
	KI_EXCEPTION_HANDLER_INIT();
	
#if defined TARGET_AMD64 || defined TARGET_I386
#define SPECIFIER "0x%02x"
#else
#define SPECIFIER "0x%08x"
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
	
	BSTATUS FaultReason = STATUS_SUCCESS;
	PKTHREAD Thread = KeGetCurrentThread();
	
	if (FaultPC <= MM_USER_SPACE_END && FaultAddress > MM_USER_SPACE_END)
		FaultReason = STATUS_ACCESS_VIOLATION;
	
	if (KeGetIPL() >= IPL_DPC)
	{
		// Page faults may only be taken at IPL_APC or lower.  This is to prevent
		// deadlock and synchronization issues.  Don't handle the page fault, but
		// instead, immediately return the specific status.
		FaultReason = STATUS_IPL_TOO_HIGH;
	}
	else
	{
		// If there are any other immediately observable bad accesses, handle them here.
		// For example, perhaps we might force the first page (0-0xFFF) to always be
		// invalid, so this condition may be added here.
		
		if (SUCCEEDED(FaultReason))
		{
			do
			{
				FaultReason = MmPageFault(FaultPC, FaultAddress, FaultMode);
			}
			while (FaultReason == STATUS_REFAULT);
		}
		
		if (SUCCEEDED(FaultReason))
		{
			KeInvalidatePage((void*) FaultAddress);
			return;
		}
		
		if (KeGetPreviousMode() == MODE_USER)
		{
			// TODO: Send a segmentation fault signal?
			// TODO: Kill this thread immediately?
		}
		
		// Invalid page fault!
		
		// Check if we are probing.
		if (Thread && Thread->Probing)
		{
			// Instead of crashing, just modify the trap frame to point to the return
			// instruction of MmProbeAddressSubEarlyReturn, and RAX to return STATUS_FAULT.
		#ifdef TARGET_AMD64
			TrapFrame->rip = (uint64_t) MmProbeAddressSubEarlyReturn;
			TrapFrame->rax = (uint64_t) STATUS_FAULT;
			return;
		#elif defined TARGET_I386
			TrapFrame->Eip = (uint32_t) MmProbeAddressSubEarlyReturn;
			TrapFrame->Eax = (uint32_t) STATUS_FAULT;
			return;
		#elif defined TARGET_ARM
			TrapFrame->Pc = (uint32_t) MmProbeAddressSubEarlyReturn;
			TrapFrame->R0 = (uint32_t) STATUS_FAULT;
			return;
		#else
			#error Hey!
		#endif
		}
	}
	
	KeCrash("invalid page fault thread=%p, ip=%p, faultaddr=%p, faultmode=%p, status=%d", Thread, FaultPC, FaultAddress, FaultMode, FaultReason);
}
