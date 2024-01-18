/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/thread.c
	
Abstract:
	This module implements the kernel thread object. Functions
	to allocate, deallocate, detach, initialize, and yield, are
	provided.
	
Author:
	iProgramInCpp - 2 October 2023
***/
#include "ki.h"

void KeYieldCurrentThreadSub(); // trap.asm

BSTATUS KiInitializeThread(PKTHREAD Thread, BIG_MEMORY_HANDLE KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	if (!Thread)
		return STATUS_INVALID_PARAMETER;
	
	if (!KernelStack)
		KernelStack = MmAllocatePoolBig(POOL_FLAG_NON_PAGED, KERNEL_STACK_SIZE / PAGE_SIZE, NULL, POOL_TAG("ThSt"));
	
	if (!KernelStack)
		return STATUS_INSUFFICIENT_MEMORY;
	
	KeInitializeDispatchHeader(&Thread->Header, DISPATCH_THREAD);
	
	Thread->StartRoutine = StartRoutine;
	Thread->StartContext = StartContext;
	
	Thread->Stack.Top    = MmGetAddressFromBigHandle(KernelStack);
	Thread->Stack.Size   = MmGetSizeFromBigHandle(KernelStack) * PAGE_SIZE;
	Thread->Stack.Handle = KernelStack;
	
	KiSetupRegistersThread(Thread);
	
	// Despite that the entry list will be part of the scheduler's thread list, it currently isn't part
	// of the scheduler. Initialize it as an empty list head so we can check it in KeSetPriorityThread.
	InitializeListHead(&Thread->EntryQueue);
	
	// Initialize other resource lists.
	InitializeListHead(&Thread->MutexList);
	
	Thread->Process = Process;
	
	// Copy defaults from process.
	Thread->Priority = Process->DefaultPriority;
	Thread->Affinity = Process->DefaultAffinity;
	
	// Mark thread as initialized.
	Thread->Status = KTHREAD_STATUS_INITIALIZED;
	
	Thread->LastProcessor = KeGetCurrentPRCB()->Id;
	Thread->DontSteal = false;
	
	// Add thread to process' thread list.
	InsertTailList(&Process->ThreadList, &Thread->EntryProc);
	
	// TODO: If the list was empty before, mark this as the main thread.
	
	return STATUS_SUCCESS;
}

PKTHREAD KeAllocateThread()
{
	PKTHREAD Thread = MmAllocatePool(POOL_FLAG_NON_PAGED, sizeof(KTHREAD));
	if (!Thread)
		return NULL;
	
	memset(Thread, 0, sizeof *Thread);
	
	Thread->Status = KTHREAD_STATUS_UNINITIALIZED;
	
	return Thread;
}

void KeDeallocateThread(PKTHREAD Thread)
{
	ASSERT(
		Thread->Status == KTHREAD_STATUS_TERMINATED ||
		Thread->Status == KTHREAD_STATUS_UNINITIALIZED);
	
	ASSERT(Thread->Stack.Handle == 0);
	
	MmFreePool(Thread);
}

void KeDetachThread(PKTHREAD Thread)
{
	// Just like with events, we don't lock the dispatcher to make sure
	// that the write is atomic - we lock the dispatcher to make sure we
	// don't actually interfere with its operation.
	KIPL Ipl = KiLockDispatcher();
	
	ASSERT(!Thread->Detached);
	Thread->Detached = true;
	
	KiUnlockDispatcher(Ipl);
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

BSTATUS KeInitializeThread(PKTHREAD Thread, BIG_MEMORY_HANDLE KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	if (!Thread)
		return STATUS_INVALID_PARAMETER;
	
	KIPL Ipl = KiLockDispatcher();
	BSTATUS Status = KiInitializeThread(Thread, KernelStack, StartRoutine, StartContext, Process);
	KiUnlockDispatcher(Ipl);
	return Status;
}