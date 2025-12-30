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

// Well, this upwards dependency sucks...
void PsOnTerminateUserThread();

void KiInitializeThread(PKTHREAD Thread, void* KernelStack, size_t KernelStackSize, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	ASSERT(KernelStack);
	
	memset (Thread, 0, sizeof *Thread);
	
	KeInitializeDispatchHeader(&Thread->Header, DISPATCH_THREAD);
	
	Thread->StartRoutine = StartRoutine;
	Thread->StartContext = StartContext;
	
	Thread->Stack.Top    = KernelStack;
	Thread->Stack.Size   = KernelStackSize;
	
	Thread->Mode = MODE_KERNEL;
	
	KiSetupRegistersThread(Thread);
	
	// Despite that the entry list will be part of the scheduler's thread list, it currently isn't part
	// of the scheduler. Initialize it as an empty list head so we can check it in KeSetPriorityThread.
	InitializeListHead(&Thread->EntryQueue);
	
	// Initialize other resource lists.
	InitializeListHead(&Thread->MutexList);
	
	Thread->Process = Process;
	
	// Copy defaults from process.
	Thread->Affinity = Process->DefaultAffinity;
	Thread->Priority = Process->DefaultPriority;
	Thread->BasePriority = Process->DefaultPriority;
	
	// Mark thread as initialized.
	Thread->Status = KTHREAD_STATUS_INITIALIZED;
	
	Thread->LastProcessor = KeGetCurrentPRCB()->Id;
	Thread->DontSteal = false;
	Thread->Probing = false;
	Thread->Suspended = true;
	
	// Add thread to process' thread list.
	InsertTailList(&Process->ThreadList, &Thread->EntryProc);
	
	// TODO: If the list was empty before, mark this as the main thread.
	InitializeListHead(&Thread->UserApcQueue);
	InitializeListHead(&Thread->KernelApcQueue);
	
	Thread->ApcDisableCount = 0;
	Thread->ApcInProgress = 0;
	
	KeInitializeTimer(&Thread->SleepTimer);
	
#ifdef DEBUG
	// When a thread spawns, it holds the dispatcher lock.
	Thread->HoldingSpinlocks = 1;
#endif
	
	Thread->TebPointer = NULL;
}

void KiSetSuspendedThread(PKTHREAD Thread, bool IsSuspended)
{
	KiAssertOwnDispatcherLock();
	
	if (Thread->Suspended == IsSuspended)
		return;
	
	Thread->Suspended = IsSuspended;
	
	if (IsSuspended)
	{
		if (KeGetCurrentThread() == Thread)
		{
			// Yield immediately.
			KiUnlockDispatcher(IPL_DPC);
			
			KeYieldCurrentThread();
			
			// Regrab the lock because it turns out we're still in
			// a function that expects us to have it when we return
			KIPL Ipl = KiLockDispatcher();
			(void) Ipl;
		}
	}
	else
	{
		if (Thread->Status == KTHREAD_STATUS_INITIALIZED)
		{
			KiReadyThread(Thread);
		}
	}
}

void KiTerminateThread(PKTHREAD Thread, KPRIORITY Increment)
{
	if (Thread->PendingTermination)
	{
		// Another termination was requested.
		return;
	}
	
	Thread->PendingTermination = true;
	Thread->IncrementTerminated = Increment;
	
	// If this thread is currently running in user mode, then it will notice
	// that this flag was set and will immediately exit.
	
	// Check if the thread is waiting for something, and the wait is alertable,
	// or initiated from user mode.
	if (Thread->Status == KTHREAD_STATUS_WAITING &&
		(Thread->WaitIsAlertable || Thread->WaitMode == MODE_USER))
	{
		KiUnwaitThread(Thread, STATUS_KILLED, Increment);
	}
}

NO_RETURN void KiTerminateUserModeThread(KPRIORITY Increment)
{
	if (KeGetCurrentThread()->IsUserThread)
		PsOnTerminateUserThread();
	
	KeTerminateThread(Increment);
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
	(void) KeRaiseIPL(IPL_DPC);
	KeGetCurrentThread()->QuantumUntil = 0;
	KiHandleQuantumEnd();
}

void KeTerminateThread2(PKTHREAD Thread, KPRIORITY Increment)
{
	if (Thread == KeGetCurrentThread())
	{
		KeTerminateThread(Increment);
		return;
	}
	
	KIPL Ipl = KiLockDispatcher();
	KiTerminateThread(Thread, Increment);
	KiUnlockDispatcher(Ipl);
}

void KeInitializeThread(PKTHREAD Thread, void* KernelStack, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	size_t KernelStackSize = MmGetSizeFromPoolAddress(KernelStack) * PAGE_SIZE;
	
	KIPL Ipl = KiLockDispatcher();
	KiInitializeThread(Thread, KernelStack, KernelStackSize, StartRoutine, StartContext, Process);
	KiUnlockDispatcher(Ipl);
}

void KeInitializeThread2(PKTHREAD Thread, void* KernelStack, size_t KernelStackSize, PKTHREAD_START StartRoutine, void* StartContext, PKPROCESS Process)
{
	KIPL Ipl = KiLockDispatcher();
	KiInitializeThread(Thread, KernelStack, KernelStackSize, StartRoutine, StartContext, Process);
	KiUnlockDispatcher(Ipl);
}

void KeSetSuspendedThread(PKTHREAD Thread, bool IsSuspended)
{
	KIPL Ipl = KiLockDispatcher();
	KiSetSuspendedThread(Thread, IsSuspended);
	KiUnlockDispatcher(Ipl);
}
