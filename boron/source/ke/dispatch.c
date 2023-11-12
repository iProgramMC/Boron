/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/dispatch.c
	
Abstract:
	This header file contains the definitions for the
	event dispatcher.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#include "ki.h"

KSPIN_LOCK KiDispatcherLock;

NO_DISCARD
KIPL KiLockDispatcher()
{
	KIPL Ipl;
	KeAcquireSpinLock(&KiDispatcherLock, &Ipl);
	return Ipl;
}

void KiUnlockDispatcher(KIPL Ipl)
{
	KeReleaseSpinLock(&KiDispatcherLock, Ipl);
}

#ifdef DEBUG

void KiAssertOwnDispatcherLock_(const char* FunctionName)
{
	if (!KiDispatcherLock.Locked)
		KeCrash("%s: dispatcher lock is unlocked", FunctionName);
}

#endif

void KeInitializeDispatchHeader(PKDISPATCH_HEADER Object, int Type)
{
	Object->Type     = Type;
	Object->Signaled = false;
	Object->ProcId   = KeGetCurrentPRCB()->Id;
	
	// Note. If a mutex, it's signaled by default until acquired.
	
	InitializeListHead(&Object->WaitBlockList);
}

bool KiIsObjectSignaled(PKDISPATCH_HEADER Header)
{
	KiAssertOwnDispatcherLock();
	
	if (Header->Type == DISPATCH_MUTEX)
		return Header->Signaled == 0;
	
	// default case
	return Header->Signaled != 0;
}

// Set the signaled state of the object and do nothing else.
static void KiSetSignaled(PKDISPATCH_HEADER Object)
{
	KiAssertOwnDispatcherLock();
	
	switch (Object->Type)
	{
		case DISPATCH_MUTEX:
		case DISPATCH_SEMAPHORE:
			Object->Signaled--;
			break;
		
		default:
			Object->Signaled = true;
	}
}

static void KiAcquireObject(PKDISPATCH_HEADER Object)
{
	KiAssertOwnDispatcherLock();
	
	switch (Object->Type)
	{
		case DISPATCH_SEMAPHORE:
			Object->Signaled++;
			break;
		
		case DISPATCH_MUTEX: {
			// TODO
			//PKMUTEX Mutex = (PKMUTEX) Object;
			Object->Signaled++;
			//Mutex->Owner = KeGetCurrentThread();
			
			break;
		}
		
		case DISPATCH_EVENT: {
			
			PKEVENT Event = (PKEVENT) Object;
			
			if (Event->Type == EVENT_SYNCHRONIZATION)
				Object->Signaled = false;
			
			// Notification events must be specifically reset with KeResetEvent.
			break;
		}
		
		default:
			// Object remains signaled
			break;
	}
}

// This routine checks if an object was satisfied enough. KiSignalObject must
// satisfy at least one wait block. A timer satisfies all of its waiters at
// expiry. A mutex satisfies only one waiter at a time. An event can satisfy
// either one or all waiters, depending on whether it's a synchronization or
// notification event.
static bool KepSatisfiedEnough(PKDISPATCH_HEADER Object, int SatisfiedCount)
{
	// Check the type of object.
	switch (Object->Type)
	{
		// By default, an object satisfies all of its waiters.
		// Timers, threads, and processes fall into this default case.
		default:
			return false;
		
		// A mutex must only be satisfied at most once.
		case DISPATCH_MUTEX:
			ASSERT(SatisfiedCount <= 1);
			return SatisfiedCount == 1;
		
		// A semaphore event can be signaled at most "Count" times.
		case DISPATCH_SEMAPHORE: {
			// @TODO
			// PKSEMAPHORE Semaphore = (PKSEMAPHORE)Object;
			
			// ASSERT(SatisfiedCount <= Semaphore->Count);
			// return SatisfiedCount == Semaphore->Count;
			return false;
		}
		// An event depends on its subtype.
		case DISPATCH_EVENT: {
			PKEVENT Event = (PKEVENT)Object;
			
			if (Event->Type == EVENT_SYNCHRONIZATION) {
				
				// A synchronization event must only be satisfied at most once.
				ASSERT(SatisfiedCount <= 1);
				return SatisfiedCount == 1;
				
			} else if (Event->Type == EVENT_NOTIFICATION) {
				
				// A notification event will allow satisfaction for all blocks.
				return false;
			}
			
			ASSERT(!"KepSatisfiedEnough - DISPATCH_EVENT - Unreachable");
			
			return false;
		}
	}
}

static void KepWaitTimerExpiry(UNUSED PKDPC Dpc, void* Context, UNUSED void* SA1, UNUSED void* SA2)
{
	// DPCs are enqueued with the dispatcher lock held.
	// @TODO Is this a bad decision? Should we not do that?
	KiAssertOwnDispatcherLock();
	
	PKTHREAD Thread = Context;
	
	if (Thread->Status != KTHREAD_STATUS_WAITING)
	{
		// TODO: It could happen when the timeout is sufficiently short and the object is
		// signalled only shortly after the timeout expired. I don't know, just return for now.
		// Hopefully this won't cause bugs of any sort..
		return;
	}
	
	// Cancel the thread's wait with a TIMEOUT status.
	// KiUnwaitThread removes the thread's wait blocks from all objects
	// it's waiting for and wakes it up.
	KiUnwaitThread(Thread, STATUS_TIMEOUT);
}

int KeWaitForMultipleObjects(
	int Count,
	void* Objects[],
	int WaitType,
	bool Alertable,
	int TimeoutMS,
	PKWAIT_BLOCK WaitBlockArray)
{
	ASSERT(WaitType == WAIT_TYPE_ALL || WaitType == WAIT_TYPE_ANY);
	
	PKTHREAD Thread = KeGetCurrentThread();
	
	int Maximum = MAXIMUM_WAIT_BLOCKS;
	
	if (!WaitBlockArray)
	{
		WaitBlockArray = Thread->WaitBlocks;
		Maximum = THREAD_WAIT_BLOCKS;
	}
	
	if (Count > Maximum)
		KeCrash("KeWaitForMultipleObjects: Object count %d is bigger than the maximum wait blocks of %d", Count, Maximum); 
	
	KIPL Ipl = KiLockDispatcher();
	
	// Perform an initial check to see if there are any objects we can acquire right away.
	// Do not enqueue the thread wait blocks on the objects' queues yet, because we're maybe
	// going to break early, or only polling(TODO). However, we are going to fill in the
	// fields of the wait block to make things easier on ourselves.
	bool Satisfied = true;
	int SatisfierIndex = 0;
	
	for (int i = 0; i < Count; i++)
	{
		PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
		PKDISPATCH_HEADER Object = Objects[i];
		
		WaitBlock->Object = Object;
		WaitBlock->Thread = Thread;
		
		bool IsSignaled = KiIsObjectSignaled(Object);
		if (IsSignaled && WaitType == WAIT_TYPE_ANY)
		{
			Satisfied = true;
			SatisfierIndex = i;
			KiAcquireObject(Object);
			break;
		}
		else if (!IsSignaled)
		{
			Satisfied = false;
		}
	}
	
	if (Satisfied && WaitType == WAIT_TYPE_ANY)
	{
		// Well, if we're already satisfied, just return immediately!
		KiUnlockDispatcher(Ipl);
		return STATUS_RANGE_WAIT + SatisfierIndex;
	}
	else if (Satisfied && WaitType == WAIT_TYPE_ALL)
	{
		// All objects are acquireable, so acquire them all.
		for (int i = 0; i < Count; i++)
		{
			KiAcquireObject(WaitBlockArray[i].Object);
		}
		
		KiUnlockDispatcher(Ipl);
		return STATUS_SUCCESS;
	}
	else if (TimeoutMS == 0)
	{
		// This is simply a poll, so return with a timeout status.
		KiUnlockDispatcher(Ipl);
		return STATUS_TIMEOUT;
	}
	
	// Ok, finally handling the non-trivial case.
	// Insert each block into the object's wait block queue.
	for (int i = 0; i < Count; i++)
	{
		PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
		PKDISPATCH_HEADER Object = Objects[i];
		
		InsertTailList(&Object->WaitBlockList, &WaitBlock->Entry);
	}
	
	Thread->WaitBlockArray = WaitBlockArray;
	Thread->WaitType = WaitType;
	Thread->WaitCount = Count;
	Thread->WaitIsAlertable = Alertable;
	Thread->WaitStatus = STATUS_WAITING;
	Thread->Status = KTHREAD_STATUS_WAITING;
	
	// Note, surely it's not zero, because we guard against that
	if (TimeoutMS != TIMEOUT_INFINITE)
	{
		// Set the wait timer up. It's going to wake us up when it expires,
		// and call KepWaitTimerExpiry, which cancels the wait.
		KeInitializeDpc(&Thread->WaitDpc, KepWaitTimerExpiry, Thread);
		KeSetImportantDpc(&Thread->WaitDpc, true);
		KeInitializeTimer(&Thread->WaitTimer);
		KiSetTimer(&Thread->WaitTimer, TimeoutMS, &Thread->WaitDpc);
	}
	
	KiUnlockDispatcher(Ipl);
	
	KeYieldCurrentThread();
	
	ASSERT(Alertable || Thread->WaitStatus != STATUS_ALERTED);
	
	return Thread->WaitStatus;
}

int KeWaitForSingleObject(void* Object, bool Alertable, int TimeoutMS)
{
	return KeWaitForMultipleObjects(1, &Object, WAIT_TYPE_ANY, Alertable, TimeoutMS, NULL);
}

bool KiSatisfyWaitBlock(PKWAIT_BLOCK WaitBlock)
{
	KiAssertOwnDispatcherLock();
	
	PKTHREAD Thread = WaitBlock->Thread;
	int Index = WaitBlock - Thread->WaitBlockArray;
	
	if (Thread->WaitType == WAIT_TYPE_ANY)
	{
		// Waiting for any object, so acquire the object and wake the thread.
		KiAcquireObject(WaitBlock->Object);
		KiCancelTimer(&Thread->WaitTimer);
		KiUnwaitThread(Thread, STATUS_RANGE_WAIT + Index);
		return true;
	}
	
	// It's waiting for all objects
	ASSERT(Thread->WaitType == WAIT_TYPE_ALL);

	// Optimistically assume we already can wake up the thread
	bool Acquirable = true;
	
	// Check if all other objects are signaled as well
	for (int i = 0; i < Thread->WaitCount; i++)
	{
		PKWAIT_BLOCK WaitBlock = &Thread->WaitBlockArray[i];
		if (!KiIsObjectSignaled(WaitBlock->Object))
		{
			Acquirable = false;
			break;
		}
	}
	
	if (!Acquirable)
		// Not all acquirable, so just return
		return false;
	
	// All acquirable, so time to acquire 'em all and return
	for (int i = 0; i < Thread->WaitCount; i++)
	{
		PKWAIT_BLOCK WaitBlock = &Thread->WaitBlockArray[i];
		KiAcquireObject(WaitBlock->Object);
	}
	
	// Dequeue the timeout timer here.
	KiCancelTimer(&Thread->WaitTimer);
	
	KiUnwaitThread(Thread, STATUS_SUCCESS);
	return true;
}

void KiSignalObject(PKDISPATCH_HEADER Object)
{
	KiSetSignaled(Object);
	
	int SatisfiedCount = 0;
	
	while (!IsListEmpty(&Object->WaitBlockList))
	{
		PLIST_ENTRY Entry = Object->WaitBlockList.Flink;
		PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, Entry);
		
		// If we could satisfy the wait block (and the thread is woken up), then
		// the WaitBlockList may be empty. We don't want to mess with it, unless
		// the WaitBlockList wasn't actually modified.
		if (!KiSatisfyWaitBlock(WaitBlock))
			RemoveHeadList(&Object->WaitBlockList);
		
		SatisfiedCount++;
		
		// If the object is a mutex and at least one object was satisfied, stop.
		if (KepSatisfiedEnough(Object, SatisfiedCount))
			return;
		
		// If the object is a semaphore, check for the Limit field of the semaphore.
	}
	
}

PKREGISTERS KiHandleSoftIpi(PKREGISTERS Regs)
{
	int Flags = KeGetPendingEvents();
	KeClearPendingEvents();
	
	KIPL Ipl = KiLockDispatcher();
	
	if (KiGetNextTimerExpiryTick() <= HalGetTickCount() + 100)
		KiDispatchTimerObjects();
	
	if (Flags & PENDING_DPCS)
		KiDispatchDpcs();
	
	if (Flags & PENDING_YIELD)
		KiPerformYield(Regs);
	
	if (KeGetCurrentPRCB()->Scheduler.NextThread)
		Regs = KiSwitchToNextThread();
	
	KiUnlockDispatcher(Ipl);
	
	return Regs;
}
