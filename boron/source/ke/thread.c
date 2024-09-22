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

void KiInitializeThread(PKTHREAD Thread, void* KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	ASSERT(KernelStack);
	
	memset (Thread, 0, sizeof *Thread);
	
	KeInitializeDispatchHeader(&Thread->Header, DISPATCH_THREAD);
	
	Thread->StartRoutine = StartRoutine;
	Thread->StartContext = StartContext;
	
	Thread->Stack.Top    = KernelStack;
	Thread->Stack.Size   = MmGetSizeFromPoolAddress(KernelStack) * PAGE_SIZE;
	
	Thread->Mode = MODE_KERNEL;
	
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
	Thread->Probing = false;
	
	// Add thread to process' thread list.
	InsertTailList(&Process->ThreadList, &Thread->EntryProc);
	
	// TODO: If the list was empty before, mark this as the main thread.
	InitializeListHead(&Thread->UserApcQueue);
	InitializeListHead(&Thread->KernelApcQueue);
	
	Thread->ApcDisableCount = 0;
	Thread->ApcInProgress = 0;
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
	
	ASSERT(Thread->Stack.Top == NULL);
	
	MmFreePool(Thread);
}

void KeSetTerminateMethodThread(PKTHREAD Thread, PKTHREAD_TERMINATE_METHOD TerminateMethod)
{
	Thread->TerminateMethod = TerminateMethod;
}

void KeYieldCurrentThread()
{
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	KeGetCurrentThread()->QuantumUntil = 0;
	KiHandleQuantumEnd(Ipl);
}

void KeInitializeThread(PKTHREAD Thread, void* KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	KIPL Ipl = KiLockDispatcher();
	KiInitializeThread(Thread, KernelStack, StartRoutine, StartContext, Process);
	KiUnlockDispatcher(Ipl);
}