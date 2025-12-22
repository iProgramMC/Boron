/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ps/userthrd.c
	
Abstract:
	This module implements the user thread management
	stuff in Boron.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#include "psp.h"

NO_RETURN
void PspUserThreadStart(void* ContextV)
{
	THREAD_START_CONTEXT Context;
	memcpy(&Context, ContextV, sizeof Context);
	MmFreePool(ContextV);
	
	// First, allocate the stack.
	void* StackAddress = NULL;
	size_t StackSize = USER_STACK_SIZE;
	BSTATUS Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&StackAddress,
		&StackSize,
		MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN,
		PAGE_READ | PAGE_WRITE
	);
	
	ASSERT(SUCCEEDED(Status) && "TODO: What happens if this fails? Maybe should have set it up earlier?!");
	
	PsGetCurrentThread()->UserStack = StackAddress;
	PsGetCurrentThread()->UserStackSize = StackSize;
	PsGetCurrentThread()->Tcb.IsUserThread = true;
	
	KeGetCurrentThread()->Mode = MODE_USER;
	KeDescendIntoUserMode(Context.InstructionPointer, (uint8_t*) StackAddress + StackSize, Context.UserContext, 0);
}

void PsOnTerminateUserThread()
{
	// We may have been in the context of an interrupt handler previously,
	// however, KiExitHardwareInterrupt will only call KiTerminateUserModeThread
	// if it was about to return to user mode, so in practice nothing should happen.
	
	// Ensure our state is ready to lock things from the memory manager.
	KeLowerIPL(IPL_NORMAL);
	ENABLE_INTERRUPTS();
	
	// Free everything that this thread needed to allocate.
	
	// TODO: What happens if the user thread partially frees its own kernel-given stack?
	// Their problem, I guess.  We honestly don't really care if this succeeds or not.
	BSTATUS Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		PsGetCurrentThread()->UserStack,
		PsGetCurrentThread()->UserStackSize,
		MEM_RELEASE
	);
	
	if (FAILED(Status))
		DbgPrint("Ps: Failed to free user stack: %d (%s)", Status, RtlGetStatusString(Status));
	
	(void) Status;
}

NO_RETURN
void OSExitThread()
{
	PsOnTerminateUserThread();
	PsTerminateThread();
}
