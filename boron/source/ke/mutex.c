/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/mutex.c
	
Abstract:
	This module implements the mutex dispatcher object.
	
Author:
	iProgramInCpp - 18 November 2023
***/
#include "ki.h"

void KiReleaseMutex(PKMUTEX Mutex)
{
	KiAssertOwnDispatcherLock();
	
	if (Mutex->OwnerThread != KeGetCurrentThread())
		KeCrash("KiReleaseMutex: mutex %p not owned by current thread %p", Mutex, KeGetCurrentThread());
	
	Mutex->Header.Signaled--;
	
	if (Mutex->Header.Signaled < MUTEX_SIGNALED)
		KeCrash("KiReleaseMutex: mutex %p was released more than possible", Mutex);
	
	// If the thread does not own the mutex at all anymore...
	if (Mutex->Header.Signaled == MUTEX_SIGNALED)
	{
		// Remove this mutex from the thread's mutex list.
		RemoveEntryList(&Mutex->MutexListEntry);
		
		// Set the owner thread of the mutex to NULL.
		Mutex->OwnerThread = NULL;
		
		// Signal the mutex object (i.e. allow another thread to acquire it), if we can.
		KiSignalObject(&Mutex->Header);
	}
}

// -------- Exposed API --------

void KeInitializeMutex(PKMUTEX Mutex, int Level)
{
	KeInitializeDispatchHeader(&Mutex->Header, DISPATCH_MUTEX);
	
#ifdef DEBUG
	Mutex->Level = Level;
#endif
	
	Mutex->Header.Signaled = MUTEX_SIGNALED;
}

int KeReadStateMutex(PKMUTEX Mutex)
{
	return AtLoad(Mutex->Header.Signaled);
}

void KeReleaseMutex(PKMUTEX Mutex)
{
	KIPL Ipl = KiLockDispatcher();
	
	KiReleaseMutex(Mutex);
	
	KiUnlockDispatcher(Ipl);
}
