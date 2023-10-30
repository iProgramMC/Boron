#include <ke.h>
#include <hal.h>

void KeInitializeTimer(PKTIMER Timer)
{
	Timer->Header.Type = DISPOBJ_TIMER;
	Timer->Header.Signaled = false;
	
	InitializeListHead(&Timer->Header.WaitBlockList);
	
	Timer->ExpiryTick = 0;
	
	Timer->IsEnqueued = false;
	
	Timer->EntryQueue.Flink =
	Timer->EntryQueue.Blink = NULL;
}

bool KeCancelTimer(PKTIMER Timer)
{
	bool Status = false;
	// Since this code is fast, disable interrupts instead of raising ipl.
	bool Restore = KeDisableInterrupts();
	
	if (Timer->IsEnqueued)
	{
		RemoveEntryList(&Timer->EntryQueue);
		Status = true;
	}
	
	KeRestoreInterrupts(Restore);
	
	return Status;
}

bool KeReadStateTimer(PKTIMER Timer)
{
	return AtLoad(Timer->Header.Signaled);
}

bool KeSetTimer(PKTIMER Timer, uint64_t DueTimeMs)
{
	bool Status = KeCancelTimer(Timer);

	// Calculate the amount of ticks we need to wait.
	uint64_t TickCount  = HalGetTickFrequency() * DueTimeMs / 1000;
	uint64_t ExpiryTick = HalGetTickCount() + TickCount;
	
	Timer->ExpiryTick = ExpiryTick;
	
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	
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
		}
		
		Entry = Entry->Flink;
	}
	
	// If we couldn't emplace it, the timer goes later than all other timer objects,
	// therefore we'll emplace it at the tail.
	InsertTailList(TimerQueue, &Timer->EntryQueue);
	
	Timer->IsEnqueued = true;
	
	KeLowerIPL(Ipl);
	
	return Status;
}

uint64_t KeGetSoonestTimerExpiry()
{
	KIPL Ipl = KeRaiseIPL(IPL_DPC);
	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;
	
	uint64_t Expiry = 0;
	
	if (!IsListEmpty(TimerQueue))
		Expiry = CONTAINING_RECORD(TimerQueue->Flink, KTIMER, EntryQueue)->ExpiryTick;
	
	KeLowerIPL(Ipl);
	
	return Expiry;
}

uint64_t KeGetNextTimerExpiryDurationInItTicks()
{
	KIPL Ipl = KeRaiseIPLIfNeeded(IPL_DPC);
	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;
	
	uint64_t Expiry = 0;
	
	if (!IsListEmpty(TimerQueue))
		Expiry = CONTAINING_RECORD(TimerQueue->Flink, KTIMER, EntryQueue)->ExpiryTick;
	
	KeLowerIPL(Ipl);
	
	if (!Expiry)
		return Expiry;
	
	uint64_t Duration = (Expiry - HalGetTickCount()) * HalGetIntTimerFrequency() / HalGetTickFrequency();
	if (Duration == 0)
		Duration = 1;
	
	return Duration;
}

void KiDispatchTimerObjects()
{
	PLIST_ENTRY TimerQueue = &KeGetCurrentScheduler()->TimerQueue;
	
	while (!IsListEmpty(TimerQueue))
	{
		PLIST_ENTRY Entry = TimerQueue->Flink;
		PKTIMER Timer = CONTAINING_RECORD(Entry, KTIMER, EntryQueue);
		
		if (Timer->ExpiryTick >= HalGetTickCount() + 100)
			break;
		
		KeSignalObject(&Timer->Header);
		
		RemoveHeadList(TimerQueue);
	}
}
