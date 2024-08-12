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
	
	// Note. If a mutex, it's signaled by default until acquired.
	
	InitializeListHead(&Object->WaitBlockList);
}

bool KiIsObjectSignaled(PKDISPATCH_HEADER Header, PKTHREAD Thread)
{
	KiAssertOwnDispatcherLock();
	
	if (Header->Type == DISPATCH_MUTEX)
	{
		if (Header->Signaled == 0)
			return true;
		
		PKMUTEX Mutex = (PKMUTEX) Header;
		
		return Mutex->OwnerThread == Thread;
	}
	
	// default case
	return Header->Signaled != 0;
}

static void KiAcquireObject(PKDISPATCH_HEADER Object, PKTHREAD Thread)
{
	KiAssertOwnDispatcherLock();
	
	switch (Object->Type)
	{
		case DISPATCH_SEMAPHORE:
			Object->Signaled--;
			break;
		
		case DISPATCH_MUTEX: {
			
			PKMUTEX Mutex = (PKMUTEX) Object;
			
			// Acquire the object.
			int OldSignaled = AtFetchAdd(Object->Signaled, 1);
			
			if (Mutex->OwnerThread != NULL && Mutex->OwnerThread != Thread)
				// Shouldn't be able to see this, by the way.
				KeCrash("KiAcquireObject: cannot acquire mutex %p already owned by someone else", Mutex);
			
			Mutex->OwnerThread = Thread;
			
			// Check if the highest level mutex has a higher level than this one.
		#ifdef DEBUG
			if (!IsListEmpty(&Thread->MutexList))
			{
				PKMUTEX HighestMutex = CONTAINING_RECORD(Thread->MutexList.Flink, KMUTEX, MutexListEntry);
				if (HighestMutex->Level > Mutex->Level)
					KeCrash("KiAcquireObject: deadlock averted - tried to acquire mutex %p with "
					        "level %d while already owning mutex %p with level %d",
					        Mutex, Mutex->Level,
					        HighestMutex, HighestMutex->Level);
			}
		#endif
			
			// Insert this mutex at the top of the mutex list.
			if (OldSignaled == MUTEX_SIGNALED)
			{
				InsertHeadList(&Thread->MutexList, &Mutex->MutexListEntry);
			}
			
			// In effect, this means that the mutex list is always ordered
			// descending, based on level.
			
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
			return Object->Signaled != 0;
		
		// A semaphore can only be acquired if it's signaled.
		case DISPATCH_SEMAPHORE:
			return Object->Signaled == 0;
		
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
	KIPL Ipl = KiLockDispatcher();
	
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
	// The thread's priority will not be boosted on timeout.
	KiUnwaitThread(Thread, STATUS_TIMEOUT, 0);
	
	KiUnlockDispatcher(Ipl);
}

// TODO: Add WaitMode parameter.
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
	
	while (true)
	{
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
			
			bool IsSignaled = KiIsObjectSignaled(Object, Thread);
			if (IsSignaled && WaitType == WAIT_TYPE_ANY)
			{
				Satisfied = true;
				SatisfierIndex = i;
				KiAcquireObject(Object, Thread);
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
				KiAcquireObject(WaitBlockArray[i].Object, Thread);
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
		Thread->WaitMode = MODE_KERNEL; // TODO
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
		
		// Yield at IPL_DPC to prevent DPCs that may unwait this thread from showing up
		// until after this thread has completely yielded.
		KiUnlockDispatcher(IPL_DPC);
		KeYieldCurrentThread();
		KeLowerIPL(Ipl);
		
		ASSERT(Alertable || Thread->WaitStatus != STATUS_ALERTED);
		
		if (Thread->WaitStatus == STATUS_ALERTED)
		{
			// TODO:
			// This is a user mode wait that was interrupted.
			// Call KeTestAlertThread and return STATUS_ALERTED.
			break;
		}
		
		ASSERT(Thread->WaitStatus != STATUS_WAITING);
		
		if (Thread->WaitStatus != STATUS_KERNEL_APC)
			break;
		
		// Request a wait again after a kernel APC.
	}
	
	return Thread->WaitStatus;
}

int KeWaitForSingleObject(void* Object, bool Alertable, int TimeoutMS)
{
	int Status = KeWaitForMultipleObjects(1, &Object, WAIT_TYPE_ANY, Alertable, TimeoutMS, NULL);
	
	// Instead of returning wait range, like multiple objects would, return success.
	if (Status == STATUS_WAIT(0))
		Status = STATUS_SUCCESS;
	
	return Status;
}

bool KiSatisfyWaitBlock(PKWAIT_BLOCK WaitBlock, KPRIORITY Increment)
{
	KiAssertOwnDispatcherLock();
	
	PKTHREAD Thread = WaitBlock->Thread;
	int Index = WaitBlock - Thread->WaitBlockArray;
	
	if (Thread->WaitType == WAIT_TYPE_ANY)
	{
		// Waiting for any object, so acquire the object and wake the thread.
		KiAcquireObject(WaitBlock->Object, WaitBlock->Thread);
		KiUnwaitThread(Thread, STATUS_RANGE_WAIT + Index, Increment);
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
		if (!KiIsObjectSignaled(WaitBlock->Object, WaitBlock->Thread))
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
		KiAcquireObject(WaitBlock->Object, WaitBlock->Thread);
	}
	
	KiUnwaitThread(Thread, STATUS_SUCCESS, Increment);
	return true;
}

PKTHREAD KiWaitTestAndGetWaiter(PKDISPATCH_HEADER Object, KPRIORITY Increment)
{
	KiAssertOwnDispatcherLock();
	
	ASSERT(KiIsObjectSignaled(Object, KeGetCurrentThread()));
	
	// TODO: Currently, nothing uses the satisfied counter. I'm not sure of its
	// reliability - it's proven not to be reliable when I was adding mutexes,
	// should I remove it?
	int SatisfiedCount = 0;
	
	PLIST_ENTRY CurrentEntry = Object->WaitBlockList.Flink;
	PLIST_ENTRY Head = &Object->WaitBlockList;
	
	PKTHREAD Result = NULL;
	
	while (CurrentEntry != Head)
	{
		PLIST_ENTRY Entry = CurrentEntry;
		CurrentEntry = CurrentEntry->Flink;
		
		PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, Entry);
		
		// If we couldn't satisfy the wait block, skip over it and use the next one.
		if (!KiSatisfyWaitBlock(WaitBlock, Increment))
			continue;
		
		// N.B. There is an interface regarding events that returns the thread
		// that was woken up by the object.  This only works for synchronization
		// events and mutexes, since only one thread will get through at a time.
		Result = WaitBlock->Thread;
		
		// If we could satisfy the wait block, the list was modified. However,
		// only "Entry" was removed. So our linked list kept its cohesion and
		// we can continue looping.
		SatisfiedCount++;
		
		// If the object is a mutex and at least one object was satisfied, stop.
		if (KepSatisfiedEnough(Object, SatisfiedCount))
			return Result;
	}
	
	return Result;
}

void KiWaitTest(PKDISPATCH_HEADER Object, KPRIORITY Increment)
{
	// Simply do the same thing as above, but drop the waiter.
	(void) KiWaitTestAndGetWaiter(Object, Increment);
}

void KeIssueSoftwareInterrupt(KIPL Ipl)
{
	PKPRCB Prcb = KeGetCurrentPRCB();
	AtOrFetch(Prcb->PendingSoftInterrupts, PENDING(Ipl));
}

void KiAcknowledgeSoftwareInterrupt(KIPL Ipl)
{
	PKPRCB Prcb = KeGetCurrentPRCB();
	AtAndFetch(Prcb->PendingSoftInterrupts, ~PENDING(Ipl));
}

void KiDispatchSoftwareInterrupts(KIPL NewIpl)
{
	bool Restore = KeDisableInterrupts();
	PKPRCB Prcb = KeGetCurrentPRCB();
	int Pending = 0;
	
	while ((Pending = Prcb->PendingSoftInterrupts & (0xFF << NewIpl)) != 0)
	{
		// Invoke the relevant software interrupt.
		if (Pending & PENDING(IPL_DPC))
			KiDpcInterrupt();
		
		if (Pending & PENDING(IPL_APC))
			KiApcInterrupt();
		
		// When handling software interrupts at IPL_APC or lower, it is totally possible
		// to be re-scheduled to a different processor/thread, so we need to re-fetch the
		// PRCB pointer.
		Prcb = KeGetCurrentPRCB();
	}
	
	Prcb->Ipl = NewIpl;
	
	KeRestoreInterrupts(Restore);
}
