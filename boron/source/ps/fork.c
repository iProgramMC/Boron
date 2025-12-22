/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	ps/fork.c
	
Abstract:
	This module implements the OSForkProcess() system call.
	
Author:
	iProgramInCpp - 18 December 2025
***/
#include "psp.h"

typedef struct {
	void* ChildReturnPC;
	void* ChildReturnSP;
	void* ThreadUserStack;
	size_t ThreadUserStackSize;
	void* PebPointer;
	HANDLE CloseProcessHandle;
}
FORK_ENTRY_CONTEXT;

NO_RETURN
void PspUserThreadStartFork(void* ContextV)
{
	FORK_ENTRY_CONTEXT* Context = ContextV;
	
	// We are in the context of the new process.  Prepare to descend into user mode.
	PsGetCurrentThread()->UserStack = Context->ThreadUserStack;
	PsGetCurrentThread()->UserStackSize = Context->ThreadUserStackSize;
	PsGetCurrentThread()->Tcb.IsUserThread = true;
	PsGetCurrentProcess()->Pcb.PebPointer = Context->PebPointer;
	
	void* ReturnPC = Context->ChildReturnPC;
	void* ReturnSP = Context->ChildReturnSP;
	OSClose(Context->CloseProcessHandle);
	MmFreePool(Context);
	
	DbgPrint("%s: Calling KeDescendIntoUserMode with ReturnPC: %p  ReturnSP: %p", __func__, ReturnPC, ReturnSP);
	KeDescendIntoUserMode(ReturnPC, ReturnSP, NULL, STATUS_IS_CHILD_PROCESS);
}

BSTATUS OSForkProcess(PHANDLE OutChildHandle, void* ChildReturnPC, void* ChildReturnSP)
{
	DbgPrint("%s:  ReturnPC: %p  ReturnSP: %p", __func__, ChildReturnPC, ChildReturnSP);
	BSTATUS Status;
	
	FORK_ENTRY_CONTEXT* ForkEntryContext = MmAllocatePool(POOL_PAGED, sizeof(FORK_ENTRY_CONTEXT));
	if (!ForkEntryContext) {
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	HANDLE ChildProcessHandle = HANDLE_NONE;
	
	// Create a new process.
	//
	// Note that we set the address mode to kernel mode so that ChildProcessHandle
	// can be written to.  If we didn't do this, the creation would fail.
	KPROCESSOR_MODE OldMode = KeSetAddressMode(MODE_KERNEL);
	Status = OSCreateProcess(
		&ChildProcessHandle,
		NULL, // ObjectAttributes
		CURRENT_PROCESS_HANDLE,
		true, // InheritHandles
		true  // DeepCloneHandles
	);
	KeSetAddressMode(OldMode);
	
	if (FAILED(Status)) {
		goto Fail1;
	}
	
	// Try and write the handle already, to avoid bearing the full cost later.
	Status = MmSafeCopy(OutChildHandle, &ChildProcessHandle, sizeof(HANDLE), KeGetPreviousMode(), true);
	if (FAILED(Status)) {
		goto Fail1;
	}
	
	// OK, the process has been created. Create a new thread. It will have a specially crafted
	// context that calls into libboron's OSDLLForkEntry, which then returns to ChildReturnPC,
	// with ChildReturnSP.
	ForkEntryContext->ChildReturnPC = ChildReturnPC;
	ForkEntryContext->ChildReturnSP = ChildReturnSP;
	
	// Since we do a deep clone of this entire process, the same address range is usable for
	// the new thread.
	ForkEntryContext->ThreadUserStack = PsGetCurrentThread()->UserStack;
	ForkEntryContext->ThreadUserStackSize = PsGetCurrentThread()->UserStackSize;
	ForkEntryContext->PebPointer = KeGetCurrentProcess()->PebPointer;
	
	// However, we DO NOT necessarily want a useless handle to this process in the child process.
	// They can just use CURRENT_PROCESS_HANDLE and it'll work the same.
	ForkEntryContext->CloseProcessHandle = ChildProcessHandle;
	
	// Reference the object by pointer
	PEPROCESS ChildProcessPointer = NULL;
	Status = ObReferenceObjectByHandle(ChildProcessHandle, PsProcessObjectType, (void**) &ChildProcessPointer);
	if (FAILED(Status)) {
		goto Fail2;
	}
	
	// Thread created.  Now, perform a deep clone of all memory regions.
	Status = MmCloneAddressSpace(ChildProcessPointer);
	if (FAILED(Status)) {
		goto Fail3;
	}
	
	// Address space cloned.  Now, create the main thread.
	// Note that we have to change the address mode here because PsCreateSystemThread is being
	// called by OSCreateThread which is the thread creation function.
	HANDLE ThreadHandle = HANDLE_NONE;
	OldMode = KeSetAddressMode(MODE_KERNEL);
	Status = PsCreateSystemThread(
		&ThreadHandle,
		NULL, // ObjectAttributes
		ChildProcessHandle,
		PspUserThreadStartFork,
		ForkEntryContext,
		false // CreateSuspended
	);
	KeSetAddressMode(OldMode);
	
	if (FAILED(Status)) {
		goto Fail3;
	}
	
	// Ok.  The fork process inside the kernel is complete.
	//
	// At this point, the new thread will jump to OSDLLForkEntry (or, really, the pointer
	// specified as parameter here), and then that function will jump down to the user space
	// program that called the fork function.
	
	// Do not free this, the new thread will do that when needed.
	ForkEntryContext = NULL;
	
	// Also don't close the child process handle on behalf of the parent process.
	// Note that we wrote the value of the handle back to userspace earlier.
	ChildProcessHandle = HANDLE_NONE;
	
	DbgPrint("AFTER FORK Status: %d", Status);
	
	ObClose(ThreadHandle);
Fail3:
	ObDereferenceObject(ChildProcessPointer);
Fail2:
	if (ChildProcessHandle != HANDLE_NONE) {
		ObClose(ChildProcessHandle);
	}
Fail1:
	if (ForkEntryContext) {
		MmFreePool(ForkEntryContext);
	}
	return Status;
}
