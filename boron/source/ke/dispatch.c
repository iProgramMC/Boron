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

int KeWaitForMultipleObjects(
	int Count,
	PKDISPATCH_HEADER Objects[],
	int WaitType,
	bool Alertable,
	PKWAIT_BLOCK WaitBlockArray)
{
	PKTHREAD Thread = KeGetCurrentThread();
	
	int Maximum = MAXIMUM_WAIT_BLOCKS;
	
	if (!WaitBlockArray)
	{
		WaitBlockArray = Thread->WaitBlocks;
		Maximum = THREAD_WAIT_BLOCKS;
	}
	
	if (Count > Maximum)
		KeCrash("KeWaitForMultipleObjects: Object count %d is bigger than the maximum wait blocks of %d", Count, Maximum); 
	
	// Raise the IPL so that we do not get interrupted while modifying the thread's wait parameters
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	
	Thread->WaitBlockArray = WaitBlockArray;
	Thread->WaitType = WaitType;
	Thread->WaitCount = Count;
	Thread->WaitIsAlertable = Alertable;
	Thread->WaitStatus = STATUS_WAITING;
	
	for (int i = 0; i < Count; i++)
	{
		PKWAIT_BLOCK WaitBlock = &WaitBlockArray[i];
		
		WaitBlock->Thread = Thread;
		WaitBlock->Object = Objects[i];
		
		// The thread's wait block should be part of the object's waiting object linked list.
		InsertTailList(&WaitBlock->Object->WaitBlockList, &WaitBlock->Entry);
	}
	
	Thread->Status = KTHREAD_STATUS_WAITING;
	
	KeLowerIPL(Ipl);
	
	KeYieldCurrentThread();
	
	return Thread->WaitStatus;
}

int KeWaitForSingleObject(PKDISPATCH_HEADER Object, bool Alertable)
{
	return KeWaitForMultipleObjects(1, &Object, WAIT_TYPE_ANY, Alertable, NULL);
}

bool KiSatisfyWaitBlock(PKWAIT_BLOCK WaitBlock)
{
	KiAssertOwnDispatcherLock();
	
	PKTHREAD Thread = WaitBlock->Thread;
	int Index = WaitBlock - Thread->WaitBlockArray;
	
	int Result = -1;
	
	if (Thread->WaitType == WAIT_TYPE_ANY)
	{
		Result = STATUS_RANGE_WAIT + Index;
	}
	else if (Thread->WaitType == WAIT_TYPE_ALL)
	{
		// Optimistically assume we already can wake up the thread
		Result = STATUS_SUCCESS;
		
		// Check if all other objects are signaled as well
		for (int i = 0; i < Thread->WaitCount; i++)
		{
			PKWAIT_BLOCK WaitBlock = &Thread->WaitBlockArray[i];
			if (!WaitBlock->Object->Signaled)
			{
				Result = -1;
				break;
			}
		}
	}
	
	if (Result != -1)
		KiUnwaitThread(Thread, Result);
	
	return Result != -1;
}

bool KiIsObjectSignaled(PKDISPATCH_HEADER Header)
{
	if (Header->Type == DISPATCH_MUTEX)
		return Header->Signaled == 0;
	
	// default case
	return Header->Signaled != 0;
}

// Set the signaled state of the object and do nothing else.
static void KiSetSignaled(PKDISPATCH_HEADER Object)
{
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
			break;
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
			
			break;
		}
	}
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
