/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	arch/amd64/thredsup.c
	
Abstract:
	This module implements the architecture specific
	thread state setup routine.
	
Author:
	iProgramInCpp - 9 October 2023
***/
#include <arch.h>
#include <ke.h>
#include <string.h>

void KiSetupRegistersThread(PKTHREAD Thread)
{
	const uint64_t OffsetFromActualBottom = 0x20;
	const uint64_t OffsetOfPKREGISTERSFromBottom = sizeof(KREGISTERS) + 0x50;
	
	uint64_t StackBottom = ((uint64_t) Thread->Stack.Top + (uint64_t) Thread->Stack.Size - OffsetFromActualBottom) & ~0xF;
	
	PKREGISTERS Regs = (PKREGISTERS) ((StackBottom - OffsetOfPKREGISTERSFromBottom) & ~0xF);
	
	memset(Regs, 0, OffsetOfPKREGISTERSFromBottom + OffsetFromActualBottom);
	
	Regs->rip = (uint64_t) Thread->StartRoutine;
	
	// The first argument to the start routine is passed
	// into rdi.
	Regs->rdi = (uint64_t) Thread->StartContext;
	
	// The stack is empty. Nothing is pushed.
	// Ensure that it is aligned to 16 bytes.
	// The function is specified not to return.
	Regs->rsp = StackBottom;
	
	// Assign the code and data segments.
	Regs->cs = SEG_RING_0_CODE;
	Regs->ds =
	Regs->es =
	Regs->fs =
	Regs->gs =
	Regs->ss = SEG_RING_0_DATA;
	
	Regs->OldIpl = IPL_NORMAL;
	
	// Enable interrupts when entering the thread.
	Regs->rflags |= 0x200;
	
	Thread->State = Regs;
}
