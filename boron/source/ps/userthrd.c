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
void PspUserThreadStart(void* Context)
{
	// Context means the initial RIP in user mode.
	
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
	
	KeDescendIntoUserMode(Context, (uint8_t*) StackAddress + StackSize);
}

NO_RETURN
BSTATUS OSExitThread()
{
	BSTATUS Status;
	
	// Free everything that this thread needed to allocate.
	
	// TODO: What happens if the user thread partially frees its own kernel-given stack?!
	//
	// Currently we don't allow that.
	Status = OSFreeVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		PsGetCurrentThread()->UserStack,
		PsGetCurrentThread()->UserStackSize,
		MEM_RELEASE
	);
	
	(void) Status;
	
	PsTerminateThread();
}

// TODO: Add OSExitProcess - exits the whole process.
// It destroys every thread!
