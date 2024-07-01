/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ke/apc.c
	
Abstract:
	This module implements the APC object and its support
	routines.
	
Author:
	iProgramInCpp - 21 February 2024
***/
#include "ki.h"
#include <hal.h>

extern int KiVectorApcIpi;
void KeIssueSoftwareInterruptApcLevel()
{
	HalRequestIpi(0, HAL_IPI_SELF, KiVectorApcIpi);
}

void KeInitializeApc(
	PKAPC Apc,
	PKTHREAD Thread,
	PKAPC_KERNEL_ROUTINE KernelRoutine,
	PKAPC_NORMAL_ROUTINE NormalRoutine,
	void* NormalContext,
	KPROCESSOR_MODE ApcMode)
{
	// The type of APC object is determined by the presence of the NormalRoutine.
	// If the NormalRoutine parameter is present, then a normal APC object is
	// initialized to run in the specified mode.
	// Otherwise, a special APC object is initialized for execution in kernel mode.
	Apc->KernelRoutine = KernelRoutine;
	Apc->NormalRoutine = NormalRoutine;
	Apc->NormalContext = NormalContext;
	Apc->ApcMode = Apc->NormalRoutine ? ApcMode : MODE_KERNEL;
	Apc->SystemArgument1 = NULL;
	Apc->SystemArgument2 = NULL;
	Apc->ListEntry.Flink = NULL;
	Apc->ListEntry.Blink = NULL;
	Apc->Enqueued = false;
	Apc->Thread = Thread;
}

bool KeInsertQueueApc(
	PKAPC Apc,
	void* SystemArgument1,
	void* SystemArgument2,
	KPRIORITY Increment)
{
	// If the APC was already inserted:
	if (Apc->Enqueued)
		return false;
	
	Apc->SystemArgument1 = SystemArgument1;
	Apc->SystemArgument2 = SystemArgument2;
	
	KIPL Ipl = KiLockDispatcher();
	PKTHREAD Thread = Apc->Thread;
	
	Apc->Enqueued = true;
	
	if (Apc->ApcMode == MODE_USER)
	{
		// Add it to the tail of the user APC queue.
		InsertTailList(&Thread->UserApcQueue, &Apc->ListEntry);
		
		if (Thread->Status == KTHREAD_STATUS_WAITING &&
		    Thread->WaitIsAlertable &&
		    Thread->WaitMode == MODE_USER &&
		    Thread->WaitStatus == STATUS_WAITING)
		{
			// A user mode APC is deliverable when the thread waits user-mode
			// alertable, and when the subject thread uses KeTestAlertThread.
			KiUnwaitThread(Thread, STATUS_ALERTED, Increment);
		}
		
		KiUnlockDispatcher(Ipl);
		return true;
	}
	
	// Place APC on subject thread's kernel mode APC queue.
	// Special APCs get priority.
	if (!Apc->NormalRoutine)
		InsertHeadList(&Thread->KernelApcQueue, &Apc->ListEntry);
	else
		InsertTailList(&Thread->KernelApcQueue, &Apc->ListEntry);
	
	if (KeGetCurrentThread() == Thread)
		KeIssueSoftwareInterruptApcLevel();
	
	if (Thread->Status == KTHREAD_STATUS_WAITING)
	{
		if (!Apc->NormalRoutine || (Thread->ApcDisableCount == 0 && !Thread->ApcInProgress))
		{
			KiUnwaitThread(Thread, STATUS_KERNEL_APC, Increment);
		}
	}
	
	KiUnlockDispatcher(Ipl);
	return true;
}

void KiDispatchApcQueue()
{
	// N.B. This only dispatches _kernel_ APCs. User APCs are dispatched somewhere else.
	ASSERT(KeGetIPL() == IPL_APC);
	KIPL Ipl = KiLockDispatcher();
	
	PKTHREAD Thread = KeGetCurrentThread();
	bool WasWaiting = Thread->WaitStatus == STATUS_KERNEL_APC;
	
	while (!IsListEmpty(&Thread->KernelApcQueue))
	{
		Thread->ApcInProgress++;
		
		PKAPC Apc = CONTAINING_RECORD(Thread->KernelApcQueue.Flink, KAPC, ListEntry);
		
		PKAPC_NORMAL_ROUTINE NormalRoutine = Apc->NormalRoutine;
		void* NormalContext = Apc->NormalContext;
		void* SystemArgument1 = Apc->SystemArgument1;
		void* SystemArgument2 = Apc->SystemArgument2;
		
		// Note! DO NOT TOUCH the APC after the special function has been called!
		// It may have been repurposed.
		
		if (!Apc->NormalRoutine)
		{
			// This is a special kernel APC.  It should always be dispatched if we got here,
			// because the APC software interrupt can never show up again.
			RemoveEntryList(&Apc->ListEntry);
			
			KiUnlockDispatcher(Ipl);
			
			Apc->KernelRoutine(
				Apc,
				&NormalRoutine,
				&NormalContext,
				&SystemArgument1,
				&SystemArgument2
			);
			
			// Note, the normal routine will not be called.
			Ipl = KiLockDispatcher();
		}
		else if (Thread->ApcInProgress == 1 && !Thread->ApcDisableCount)
		{
			// This is a normal kernel APC.  They are only dispatched if APCs
			// aren't disabled, and we aren't already processing an APC.
			RemoveEntryList(&Apc->ListEntry);
			
			KiUnlockDispatcher(Ipl);
			
			Apc->KernelRoutine(
				Apc,
				&NormalRoutine,
				&NormalContext,
				&SystemArgument1,
				&SystemArgument2
			);
			
			if (NormalRoutine)
			{
				// NOTE: This might seem like a layering violation!  However, it's
				// totally OK, since ApcInProgress is incremented when handling an
				// APC.  So if the normal APC were to enqueue another APC, and the
				// enqueue function were to request the IPL_APC interrupt again it
				// would exit right away.
				KeLowerIPL(IPL_NORMAL);
				
				Apc->NormalRoutine(
					NormalContext,
					SystemArgument1,
					SystemArgument2
				);
				
				KeRaiseIPL(IPL_APC);
			}
			
			Ipl = KiLockDispatcher();
		}
		else
		{
			ASSERT(Thread->ApcInProgress != 0);
			
			// Surely no special APCs are enqueued _after_ a normal APC, right?
			// If that somehow is the case.... crap.
			Thread->ApcInProgress--;
			break;
		}
		
		Thread->ApcInProgress--;
	}
	
	// If this thread was woken up from a wait to perform this APC, then let the wait function
	// know that it was woken up by a kernel APC.  This way, it can retry the wait.
	if (WasWaiting)
	{
		Thread->WaitStatus = STATUS_KERNEL_APC;
	}
	
	KiUnlockDispatcher(Ipl);
}
