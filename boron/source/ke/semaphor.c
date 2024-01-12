/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/semaphor.c
	
Abstract:
	This module implements the kernel semaphore dispatcher
	object. It provides initialize, read state, and release
	functions.
	
	N.B. Acquiring a semaphore is done by performing a wait
	operation on it.
	
Author:
	iProgramInCpp - 18 November 2023
***/
#include "ki.h"

void KiReleaseSemaphore(PKSEMAPHORE Semaphore, int Adjustment)
{
	ASSERT(Adjustment > 0);
	KiAssertOwnDispatcherLock();
	
	// Increment the signaled state of the object.
	Semaphore->Header.Signaled += Adjustment;
	
	if (Semaphore->Header.Signaled < 0)
		KeCrash("KiReleaseSemaphore: overflow");
	
	// Assert that we are signaled.
	// A signal state of less than zero is a bug.
	ASSERT(Semaphore->Header.Signaled > 0);
	
	// Signal the object - maybe we'll wake something up.
	KiWaitTest(&Semaphore->Header);
}

// -------- Exposed API --------

void KeInitializeSemaphore(PKSEMAPHORE Semaphore, int Count, int Limit)
{
	KeInitializeDispatchHeader(&Semaphore->Header, DISPATCH_SEMAPHORE);
	
	Semaphore->Header.Signaled = Count;
	
	Semaphore->Limit = Limit;
}

int KeReadStateSemaphore(PKSEMAPHORE Semaphore)
{
	ASSERT_SEMAPHORE(Semaphore);
	
	return AtLoad(Semaphore->Header.Signaled);
}

void KeReleaseSemaphore(PKSEMAPHORE Semaphore, int Adjustment)
{
	ASSERT_SEMAPHORE(Semaphore);
	KIPL Ipl = KiLockDispatcher();
	KiReleaseSemaphore(Semaphore, Adjustment);
	KiUnlockDispatcher(Ipl);
}