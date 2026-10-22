/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	ke/fmutex.c

Abstract:
	This module implements

Author:
	iProgramInCpp - 22 October 2026
***/
#include <ke.h>

#define FAST_MUTEX_PRIORITY_INCREMENT 2

void KeInitializeFastMutex(PFAST_MUTEX Mutex)
{
	Mutex->Count = 1;
	Mutex->OwnerThread = NULL;
	Mutex->OwnerCount = 0;

	KeInitializeEvent(&Mutex->Event, EVENT_SYNCHRONIZATION, false);

#ifdef DEBUG
	Mutex->Contention = 0;
#endif
}

void KeAcquireFastMutex(PFAST_MUTEX Mutex)
{
	PKTHREAD CurrentThread = KeGetCurrentThread();

	// Note: This seems racy, but in practice it's either "CurrentThread"
	// (which would not change) or something else we don't actually care
	// about.
	if (AtLoadMO(Mutex->OwnerThread, ATOMIC_MEMORD_RELAXED) == CurrentThread)
	{
		Mutex->OwnerCount++;
		return;
	}

	intptr_t Count = AtAddFetchMO(Mutex->Count, -1, ATOMIC_MEMORD_ACQUIRE);

	if (Count != 0)
	{
#ifdef DEBUG
		AtAddFetchMO(Mutex->Contention, 1, ATOMIC_MEMORD_ACQUIRE);
#endif

		// Could not acquire ownership, do it the slow way.
		UNUSED BSTATUS Status;
		Status = KeWaitForSingleObject(&Mutex->Event, false, TIMEOUT_INFINITE, MODE_KERNEL);
		ASSERT(SUCCEEDED(Status) && "Fast mutexes may never fail to be acquired.");
	}

	Mutex->OwnerThread = KeGetCurrentThread();
	Mutex->OwnerCount++;
}

void KeReleaseFastMutex(PFAST_MUTEX Mutex)
{
	PKTHREAD CurrentThread = KeGetCurrentThread();

#ifdef DEBUG
	if (AtLoadMO(Mutex->OwnerThread, ATOMIC_MEMORD_RELAXED) != CurrentThread)
	{
		KeCrash("Mutex %p owned by thread %p, not %p", Mutex, Mutex->OwnerThread, CurrentThread);
	}
#endif

	if (--Mutex->OwnerCount > 0)
	{
		return;
	}

	Mutex->OwnerThread = NULL;

	intptr_t Count = AtAddFetchMO(Mutex->Count, 1, ATOMIC_MEMORD_RELEASE);

	if (Count == 1)
	{
		// Nobody is waiting, just return.
		return;
	}

	// Somebody is waiting.
	KeSetEvent(&Mutex->Event, FAST_MUTEX_PRIORITY_INCREMENT);
}

bool KeTryToAcquireFastMutex(PFAST_MUTEX Mutex)
{
	PKTHREAD CurrentThread = KeGetCurrentThread();

	if (Mutex->OwnerThread == CurrentThread)
	{
		Mutex->OwnerCount++;
		return true;
	}

	intptr_t Expected = 1;
	if (!AtCompareExchangeMO(&Mutex->Count, &Expected, 0, ATOMIC_MEMORD_ACQUIRE, ATOMIC_MEMORD_RELAXED))
	{
		// Could not acquire.
		return false;
	}

	Mutex->OwnerThread = KeGetCurrentThread();
	Mutex->OwnerCount++;
	return true;
}
