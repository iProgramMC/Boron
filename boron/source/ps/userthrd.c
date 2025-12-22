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
	if (FAILED(Status)) {
		PsTerminateThread();
	}
	
	PsGetCurrentThread()->UserStack = StackAddress;
	PsGetCurrentThread()->UserStackSize = StackSize;
	PsGetCurrentThread()->Tcb.IsUserThread = true;
	
	uint32_t* StackBottom = (uint32_t*)((uintptr_t)StackAddress + StackSize);
	
	// We have to do this on i386 because of ABI constraints.  Other platforms are saner
	// and allow parameter passing through registers. As such, this platform is the only
	// one where this hack is required.
#ifdef TARGET_I386
	// Put the user context onto the stack, as well as a fake return address.
	// This is so that we can pass the context as a parameter.
	struct {
		uint32_t ReturnAddr;
		uint32_t Parameter;
	}
	EntryData;
	EntryData.ReturnAddr = 0;
	EntryData.Parameter = (uint32_t) Context.UserContext;
	
	StackBottom -= 2;
	Status = MmSafeCopy(StackBottom, &EntryData, sizeof EntryData, MODE_USER, true);
	if (FAILED(Status))
	{
		DbgPrint("Failed to write entry context for the new thread, just exiting.");
		OSExitThread();
	}
#endif
	
	KeGetCurrentThread()->Mode = MODE_USER;
#ifdef TARGET_I386
	KeDescendIntoUserMode(Context.InstructionPointer, StackBottom, 0);
#else
	KeDescendIntoUserMode(Context.InstructionPointer, StackBottom, Context.UserContext, 0);
#endif
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
