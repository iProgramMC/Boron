/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/thread.c
	
Abstract:
	This header file contains the implementation of the
	thread object.
	
Author:
	iProgramInCpp - 2 October 2023
***/
#include "ki.h"

void KeYieldCurrentThreadSub(); // trap.asm

PKTHREAD KeCreateEmptyThread()
{
	PKTHREAD Thread = ExAllocateSmall(sizeof(KTHREAD));
	if (!Thread)
		return NULL;
	
	memset(Thread, 0, sizeof *Thread);
	
	Thread->Status = KTHREAD_STATUS_UNINITIALIZED;
	
	return Thread;
}

// TODO: Process Parameter at the end (PKPROCESS Process)
void KeInitializeThread(PKTHREAD Thread, EXMEMORY_HANDLE KernelStack, PKTHREAD_START StartRoutine, void* StartContext)
{
	KeInitializeDispatchHeader(&Thread->Header, DISPATCH_THREAD);
	
	if (!KernelStack)
		KernelStack = ExAllocatePool(POOL_FLAG_NON_PAGED, KERNEL_STACK_SIZE / PAGE_SIZE, NULL, EX_TAG("ThSt"));
	
	Thread->StartRoutine = StartRoutine;
	Thread->StartContext = StartContext;
	
	Thread->Stack.Top    = ExGetAddressFromHandle(KernelStack);
	Thread->Stack.Size   = ExGetSizeFromHandle(KernelStack) * PAGE_SIZE;
	Thread->Stack.Handle = KernelStack;
	
	KiSetupRegistersThread(Thread);
	
	// Despite that the entry list will be part of the scheduler's thread list, it currently isn't part
	// of the scheduler. Initialize it as an empty list head so we can check it in KeSetPriorityThread.
	InitializeListHead(&Thread->EntryQueue);
	
	Thread->Status = KTHREAD_STATUS_INITIALIZED;
}

void KeYieldCurrentThread()
{
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	
	KeGetCurrentThread()->QuantumUntil = 0;
	
	// N.B. we probably don't need to update the scheduler's
	// QuantumUntil field because the pending event stuff doesn't
	// _really_ care about that.
	
	KeSetPendingEvent(PENDING_YIELD);
	KeYieldCurrentThreadSub();
	
	KeLowerIPL(Ipl);
}
