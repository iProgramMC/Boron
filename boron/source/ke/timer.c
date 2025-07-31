/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/timer.c
	
Abstract:
	This module file implements the Timer dispatcher object.
	
Author:
	iProgramInCpp - 30 October 2023
***/
#include "ki.h"

#define TIMER_EXPIRE_INCREMENT 2 /* Increment when a timer expires */

// XXX: A tree is better in theory, but is it really in practice?

bool KiRemoveTimerTree(PKTIMER Timer)
{
	return RemoveItemRbTree(&KiGetCurrentScheduler()->TimerTree, &Timer->EntryTree);
}

bool KiInsertTimerTree(PKTIMER Timer)
{
	// XXX: What if the key is 32-bit for some reason? Should the key stay 64-bit instead?
	Timer->EntryTree.Key = Timer->ExpiryTick;
	
	return InsertItemRbTree(&KiGetCurrentScheduler()->TimerTree, &Timer->EntryTree);
}

bool KiCancelTimer(PKTIMER Timer)
{
	KiAssertOwnDispatcherLock();
	bool Status = Timer->IsEnqueued;
	
	// Note. "IF" check not necessary, but good for an optimization.
	if (Status)
		KiRemoveTimerTree(Timer);
	
	Timer->IsEnqueued = false;
	
	return Status;
}

bool KiSetTimer(PKTIMER Timer, uint64_t DueTimeMs, PKDPC Dpc)
{
	KiAssertOwnDispatcherLock();
	
	bool Status = Timer->IsEnqueued;
	if (Status) {
		DbgPrint("KiSetTimer: Timer is already enqueued, removing");
		KiRemoveTimerTree(Timer);
	}

	// Calculate the amount of ticks we need to wait.
	uint64_t TickCount  = HalGetTickFrequency() * DueTimeMs / 1000;
	uint64_t ExpiryTick = HalGetTickCount() + TickCount;
	
	Timer->ExpiryTick = ExpiryTick;
	
	// Enqueue the timer in the timer tree.
	bool InsertStatus = KiInsertTimerTree(Timer);
	
	while (UNLIKELY(!InsertStatus))
	{
		// Wow! The timer couldn't be inserted due to a clash. Surprising as
		// the TSC's resolution is quite fine.  Give it an ever so slight upper hand.
		Timer->ExpiryTick++;
		InsertStatus = KiInsertTimerTree(Timer);
	}
	
	Timer->IsEnqueued = true;
	
	Timer->Dpc = Dpc;
	
	return Status;
}

uint64_t KiGetNextTimerExpiryTick()
{
	KiAssertOwnDispatcherLock();
	
	PRBTREE TimerTree = &KiGetCurrentScheduler()->TimerTree;
	
	uint64_t Expiry = 0;
	
	if (!IsEmptyRbTree(TimerTree))
	{
		PRBTREE_ENTRY Entry = GetFirstEntryRbTree(TimerTree);
		
		Expiry = CONTAINING_RECORD(Entry, KTIMER, EntryTree)->ExpiryTick;
	}
	
	return Expiry;
}

uint64_t KiGetNextTimerExpiryItTick()
{
	KiAssertOwnDispatcherLock();
	
	PRBTREE TimerTree = &KiGetCurrentScheduler()->TimerTree;
	
	uint64_t Expiry = 0;
	
	if (!IsEmptyRbTree(TimerTree))
	{
		PRBTREE_ENTRY Entry = GetFirstEntryRbTree(TimerTree);
		
		Expiry = CONTAINING_RECORD(Entry, KTIMER, EntryTree)->ExpiryTick;
	}
	
	if (!Expiry)
		return Expiry;
	
	uint64_t Duration = (Expiry - HalGetTickCount()) * HalGetIntTimerFrequency() / HalGetTickFrequency();
	if (Duration == 0)
		Duration = 1;
	
	return Duration;
}

void KiDispatchTimerObjects()
{
	KiAssertOwnDispatcherLock();
	
	PRBTREE TimerTree = &KiGetCurrentScheduler()->TimerTree;
	
	while (!IsEmptyRbTree(TimerTree))
	{
		PRBTREE_ENTRY Entry = GetFirstEntryRbTree(TimerTree);
		
		PKTIMER Timer = CONTAINING_RECORD(Entry, KTIMER, EntryTree);
		
		if (Timer->ExpiryTick >= HalGetTickCount() + 100)
			break;
		
		// Enqueue the DPC associated with the timer, if needed
		if (Timer->Dpc)
			KeEnqueueDpc(Timer->Dpc, NULL, NULL);
		
		Timer->Header.Signaled = true;
		Timer->IsEnqueued = false;
		KiRemoveTimerTree(Timer);
		
		KiWaitTest(&Timer->Header, TIMER_EXPIRE_INCREMENT);
	}
}

void KiDispatchTimerQueue()
{
	KIPL Ipl = KiLockDispatcher();
	
	if (KiGetNextTimerExpiryTick() <= HalGetTickCount() + 100)
		KiDispatchTimerObjects();
	
	KiUnlockDispatcher(Ipl);
}

// -------- Exposed API --------

bool KeReadStateTimer(PKTIMER Timer)
{
	ASSERT_TIMER(Timer);
	return AtLoad(Timer->Header.Signaled);
}

void KeInitializeTimer(PKTIMER Timer)
{
	KeInitializeDispatchHeader(&Timer->Header, DISPATCH_TIMER);
	
	Timer->ExpiryTick = 0;
	
	Timer->IsEnqueued = false;
	
	InitializeRbTreeEntry(&Timer->EntryTree);
}

bool KeCancelTimer(PKTIMER Timer)
{
	KIPL Ipl = KiLockDispatcher();
	bool Status = KiCancelTimer(Timer);
	KiUnlockDispatcher(Ipl);
	return Status;
}

bool KeSetTimer(PKTIMER Timer, uint64_t DueTimeMs, PKDPC Dpc)
{
	KIPL Ipl = KiLockDispatcher();
	bool Status = KiSetTimer(Timer, DueTimeMs, Dpc);
	KiUnlockDispatcher(Ipl);
	return Status;
}
