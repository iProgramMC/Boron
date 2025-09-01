/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/event.c
	
Abstract:
	This module implements the event dispatcher object.
	
Author:
	iProgramInCpp - 13 November 2023
***/
#include "ki.h"

void KiSetEvent(PKEVENT Event, KPRIORITY Increment)
{
	KiAssertOwnDispatcherLock();
	
	Event->Header.Signaled = true;
	
	KiWaitTest(&Event->Header, Increment);
}

PKTHREAD KiSetEventAndGetWaiter(PKEVENT Event, KPRIORITY Increment)
{
#ifdef DEBUG
	if (Event->Type != EVENT_SYNCHRONIZATION)
		KeCrash("KiSetEventAndGetWaiter: Cannot call this interface on a notification event!");
#endif
	
	KiAssertOwnDispatcherLock();
	
	Event->Header.Signaled = true;
	
	return KiWaitTestAndGetWaiter(&Event->Header, Increment);
}

void KiResetEvent(PKEVENT Event)
{
	// NOTE: We don't assert that we own the dispatcher
	// lock just to make sure that the signaled variable
	// is set atomically. We do it so that the dispatcher
	// doesn't suddenly wake up with the event unsignaled
	// when it should have been signaled.
	KiAssertOwnDispatcherLock();
	
	Event->Header.Signaled = false;
}

// -------- Exposed API --------

void KeInitializeEvent(PKEVENT Event, int EventType, bool State)
{
	KeInitializeDispatchHeader(&Event->Header, DISPATCH_EVENT);
	
	ASSERT(EventType == EVENT_NOTIFICATION ||
	       EventType == EVENT_SYNCHRONIZATION);
	
	Event->Type = EventType;
	Event->Header.Signaled = State;
}

bool KeReadStateEvent(PKEVENT Event)
{
	ASSERT_EVENT(Event);
	return AtLoad(Event->Header.Signaled);
}

void KeSetEvent(PKEVENT Event, KPRIORITY Increment)
{
	KIPL Ipl = KiLockDispatcher();
	
	KiSetEvent(Event, Increment);
	
	KiUnlockDispatcher(Ipl);
}

void KeResetEvent(PKEVENT Event)
{
	KIPL Ipl = KiLockDispatcher();
	
	KiResetEvent(Event);
	
	KiUnlockDispatcher(Ipl);
}

void KePulseEvent(PKEVENT Event, KPRIORITY Increment)
{
	KIPL Ipl = KiLockDispatcher();
	
	KiSetEvent(Event, Increment);
	KiResetEvent(Event);
	
	KiUnlockDispatcher(Ipl);
}

// NOTE: This API is to be called at IPL_DPC.
// Since this API is only used in the rwlock implementation, and it holds
// the rwlock's spinlock, which raises IPL to DPC-level, it's fine to use.
PKTHREAD KeSetEventAndGetWaiter(PKEVENT Event, KPRIORITY Increment)
{
	KIPL Ipl = KiLockDispatcher();
	ASSERT(Ipl == KeGetIPL());
	
	PKTHREAD Result = KiSetEventAndGetWaiter(Event, Increment);
	KiUnlockDispatcher(Ipl);
	
	return Result;
}

// These APIs are used when the call IMMEDIATELY precedes a kernel Wait function call.
// This ensures that the event setting and the wait are performed atomically.
void KeSetEventWait(PKEVENT Event, KPRIORITY Increment)
{
	KiLockDispatcherWait();
	KiSetEvent(Event, Increment);
}

void KeResetEventWait(PKEVENT Event)
{
	KiLockDispatcherWait();
	KiResetEvent(Event);
}

void KePulseEventWait(PKEVENT Event, KPRIORITY Increment)
{
	KiLockDispatcherWait();
	KiSetEvent(Event, Increment);
	KiResetEvent(Event);
}

