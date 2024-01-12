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

bool KiCancelTimer(PKTIMER Timer)
{
	bool Status = Timer->IsEnqueued;
	
	if (Status)
		RemoveEntryList(&Timer->EntryQueue);
	
	Timer->IsEnqueued = false;
	
	return Status;
}

bool KiSetTimer(PKTIMER Timer, uint64_t DueTimeMs, PKDPC Dpc)
{
	KiAssertOwnDispatcherLock();
	
	bool Status = Timer->IsEnqueued;
	if (Status) {
		DbgPrint("KiSetTimer: Timer is already enqueued, removing");
		RemoveEntryList(&Timer->EntryQueue);
	}

	// Calculate the amount of ticks we need to wait.
	uint64_t TickCount  = HalGetTickFrequency() * DueTimeMs / 1000;
	uint64_t ExpiryTick = HalGetTickCount() + TickCount;
	
	Timer->ExpiryTick = ExpiryTick;
	
	bool WasEmplaced = false;

	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;

	// Look for a suitable place to enqueue the timer.
	PLIST_ENTRY Entry = TimerQueue->Flink;
	while (Entry != TimerQueue && !WasEmplaced)
	{
		// If the current timer object expires later than the new one,
		// place the new one before the current one.
		PKTIMER TimerCurrent = CONTAINING_RECORD(Entry, KTIMER, EntryQueue);

		if (TimerCurrent->ExpiryTick > Timer->ExpiryTick)
		{
			InsertTailList(&TimerCurrent->EntryQueue, &Timer->EntryQueue);
			WasEmplaced = true;
			break;
		}

		Entry = Entry->Flink;
	}

	// If we couldn't emplace it, the timer goes later than all other timer objects,
	// therefore we'll emplace it at the tail.
	if (!WasEmplaced)
		InsertTailList(TimerQueue, &Timer->EntryQueue);
	
	Timer->IsEnqueued = true;
	
	Timer->Dpc = Dpc;
	
	return Status;
}

uint64_t KiGetNextTimerExpiryTick()
{
	KiAssertOwnDispatcherLock();
	
	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;
	
	uint64_t Expiry = 0;
	if (!IsListEmpty(TimerQueue))
		Expiry = CONTAINING_RECORD(TimerQueue->Flink, KTIMER, EntryQueue)->ExpiryTick;
	
	return Expiry;
}

uint64_t KiGetNextTimerExpiryItTick()
{
	KiAssertOwnDispatcherLock();
	
	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;
	
	uint64_t Expiry = 0;
	
	if (!IsListEmpty(TimerQueue))
		Expiry = CONTAINING_RECORD(TimerQueue->Flink, KTIMER, EntryQueue)->ExpiryTick;
	
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
	
	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;
	
#ifdef DEBUG
	//size_t TreeSize = ExGetItemCountAaTree(TimerTree), TreeHeight = ExGetHeightAaTree(TimerTree);
	//DbgPrint("Tree Size %zu And Height %zu", TreeSize, TreeHeight);
#endif
	
	while (!IsListEmpty(TimerQueue))
	{
		PLIST_ENTRY Entry = TimerQueue->Flink;
		PKTIMER Timer = CONTAINING_RECORD(Entry, KTIMER, EntryQueue);
		
		if (Timer->ExpiryTick >= HalGetTickCount() + 100)
			break;
		
		// Enqueue the DPC associated with the timer, if needed
		if (Timer->Dpc)
			KeEnqueueDpc(Timer->Dpc, NULL, NULL);
		
		Timer->Header.Signaled = true;
		KiWaitTest(&Timer->Header);
		
		Timer->IsEnqueued = false;
		
		RemoveHeadList(TimerQueue);
	}
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

	Timer->EntryQueue.Flink =
	Timer->EntryQueue.Blink = NULL;
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
