/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ke/thread.c
	
Abstract:
	This module implements the read-write lock. Acquiring
	it shared means one intends to read from the structure
	guarded by the lock, and acquiring it exclusive means
	one intends to modify the structure.
	
Author:
	iProgramInCpp - 10 January 2024
***/
#include "exp.h"

#define EX_RWLOCK_WAIT_TIMEOUT 300 /* Milliseconds */

PEX_RW_LOCK_OWNER ExFindOwnerRwLock(PEX_RW_LOCK Lock, PKTHREAD Thread, KIPL Ipl);

BSTATUS ExWaitOnRwLock(PEX_RW_LOCK Lock, void* EventObject);

void ExInitializeRwLock(PEX_RW_LOCK Lock)
{
	KeInitializeSpinLock(&Lock->GuardLock);
	KeInitializeEvent(&Lock->ExclusiveSyncEvent, EVENT_SYNCHRONIZATION, false);
	KeInitializeSemaphore(&Lock->SharedSemaphore, 0, SEMAPHORE_LIMIT_NONE);
	
	memset(&Lock->ExclusiveOwner, 0, sizeof(EX_RW_LOCK_OWNER));
	memset(&Lock->SharedOwnerPreall, 0, sizeof(EX_RW_LOCK_OWNER));
	InitializeListHead(&Lock->SharedOwners);
	Lock->SharedOwnerCount = 0;
	Lock->SharedWaiterCount = 0;
	Lock->ExclusiveWaiterCount = 0;
	Lock->HeldCount = 0;
}

// Returns:
//  If DontBlock:
//     STATUS_SUCCESS - Lock was acquired
//     STATUS_TIMEOUT - Lock would block if acquired
//  Else:
//     STATUS_SUCCESS - Lock was acquired
//     STATUS_ALERTED - Alertable was true and wait was alerted miscellaneously
//     STATUS_KILLED  - Alertable was true and thread was killed
BSTATUS ExAcquireExclusiveRwLock(PEX_RW_LOCK Lock, bool DontBlock, bool Alertable)
{
	PKTHREAD CurrentThread = KeGetCurrentThread();
	
	KIPL Ipl;
	KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
	
	if (Lock->HeldCount == 0)
	{
		Lock->HeldCount = 1;
		
		Lock->ExclusiveOwner.Locked = true;
		Lock->ExclusiveOwner.OwnerThread = CurrentThread;
		// Won't initialize the list entry because it's not part of a list.
		
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		return STATUS_SUCCESS;
	}
	
	if (Lock->ExclusiveOwner.Locked != 0)
	{
		if (Lock->ExclusiveOwner.OwnerThread == CurrentThread)
		{
			// Yoink!
			Lock->HeldCount += 1;
			
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			return STATUS_SUCCESS;
		}
	}
	else if (DontBlock)
	{
		// HeldCount is not zero, and lock isn't acquired exclusively,
		// return because the lock couldn't be acquired without blocking
		return STATUS_TIMEOUT;
	}
	
	if (Alertable)
	{
		// TODO: Check if we're killed.
	}
	
	Lock->ExclusiveWaiterCount++;
	KeReleaseSpinLock(&Lock->GuardLock, Ipl);
	
	BSTATUS Status = ExWaitOnRwLock(Lock, &Lock->ExclusiveSyncEvent);
	
#ifdef DEBUG
	if (Status)
		KeCrash("ExAcquireExclusiveRwLock: ExWaitOnRwLock failed!");
#endif
	
	Lock->ExclusiveOwner.OwnerThread = CurrentThread;
	
	if (Alertable)
	{
		// TODO: Check if the thread was killed and release if so.
	}
	
	return Status;
}

// Returns:
//  If DontBlock:
//     STATUS_SUCCESS - Lock was acquired
//     STATUS_TIMEOUT - Lock would block if acquired
//  Else:
//     STATUS_SUCCESS - Lock was acquired
//     STATUS_ALERTED - Alertable was true and wait was alerted miscellaneously
//     STATUS_KILLED  - Alertable was true and thread was killed
//     *crash*        - Lock couldn't be acquired, but alertable is false
// CanStarve - Whether we have priority over exclusive waiters
BSTATUS ExAcquireSharedRwLock(PEX_RW_LOCK Lock, bool DontBlock, bool Alertable, bool CanStarve)
{
	PKTHREAD CurrentThread = KeGetCurrentThread();
	
	while (true)
	{
		KIPL Ipl;
		KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
		
		if (Lock->HeldCount == 0)
		{
			// No shared owners, so use the preallocated one.
			Lock->SharedOwnerPreall.Locked = true;
			Lock->SharedOwnerPreall.OwnerThread = CurrentThread;
			InsertTailList(&Lock->SharedOwners, &Lock->SharedOwnerPreall.Entry);
			
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			return STATUS_SUCCESS;
		}
		
		PEX_RW_LOCK_OWNER Owner = NULL;
		
		if (Lock->ExclusiveOwner.Locked)
		{
			if (Lock->ExclusiveOwner.OwnerThread == CurrentThread)
			{
				// Yoink!
				//
				// Acquire it as if it were exclusive.  This is so that
				// ExReleaseRwLock can behave the same way and we don't
				// need to introduce special cases.
				Lock->ExclusiveOwner.Locked++;
				
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				return STATUS_SUCCESS;
			}
			
			Owner = ExFindOwnerRwLock(Lock, NULL, Ipl);
			
			if (!Owner)
			{
				// Try again!
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				continue;
			}
		}
		else
		{
			Owner = ExFindOwnerRwLock(Lock, CurrentThread, Ipl);
			
			if (!Owner)
			{
				// Try again!
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				continue;
			}
			
			if (CurrentThread == Owner->OwnerThread)
			{
				// Yoink recursively?
				// This is probably not needed but do it anyway
				Owner->Locked++;
				
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				return STATUS_SUCCESS;
			}
			
			if (CanStarve || Lock->ExclusiveWaiterCount == 0)
			{
				// Yoink!
				Owner->OwnerThread = CurrentThread;
				Owner->Locked = 1;
				Lock->HeldCount++;
				
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				return STATUS_SUCCESS;
			}
			else if (DontBlock)
			{
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				return STATUS_TIMEOUT;
			}
		}
		
		if (Alertable)
		{
			// TODO: We have to wait, so check if we got killed.
		}
		
		Owner->OwnerThread = CurrentThread;
		Owner->Locked = 1;
		Lock->SharedWaiterCount++;
		
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		break;
	}
	
	// Wait!
	BSTATUS Status = ExWaitOnRwLock(Lock, &Lock->SharedSemaphore);
	
#ifdef DEBUG
	if (Status)
		KeCrash("ExAcquireExclusiveShared: ExWaitOnRwLock failed!");
#endif
	
	if (Alertable)
	{
		// TODO: Check if the thread was killed and release if so.
	}
	
	return Status;
}

void ExDemoteToSharedRwLock(PEX_RW_LOCK Lock)
{
	while (true)
	{
		KIPL Ipl;
		KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
		
	#ifdef DEBUG
		if (Lock->ExclusiveOwner.OwnerThread != KeGetCurrentThread())
			KeCrash("ExDemoteToSharedRwLock: Current thread does not own lock exclusively");
	#endif
		
		// Find a slot to put it in.
		PEX_RW_LOCK_OWNER Owner = ExFindOwnerRwLock(Lock, NULL, Ipl);
		
		if (!Owner)
		{
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			continue;
		}
		
		Lock->ExclusiveOwner.OwnerThread = NULL;
		
		Owner->OwnerThread = KeGetCurrentThread();
		Owner->Locked = Lock->ExclusiveOwner.Locked;
		
		Lock->ExclusiveOwner.Locked = 0;
		
		int Waiters = Lock->SharedWaiterCount;
		
		if (Waiters)
		{
			Lock->HeldCount += Waiters;
			Lock->SharedWaiterCount = 0;
			
			KeReleaseSemaphore(&Lock->SharedSemaphore, Waiters);
		}
		
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		break;
	}
}

BSTATUS ExWaitOnRwLock(UNUSED PEX_RW_LOCK Lock, void* EventObject)
{
	while (true)
	{
		BSTATUS Status = KeWaitForSingleObject(EventObject, false, EX_RWLOCK_WAIT_TIMEOUT);
		
		if (Status == STATUS_TIMEOUT)
			return Status;
		
		// Try to boost priority of other owners to get them out of this lock:
		// TODO
	}
	
	return STATUS_SUCCESS;
}

PEX_RW_LOCK_OWNER ExFindOwnerRwLock(PEX_RW_LOCK Lock, PKTHREAD Thread, KIPL Ipl)
{
#ifdef DEBUG
	if (!Lock->GuardLock.Locked)
		KeCrash("ExFindOwnerRwLock: Rwlock's spinlock is unlocked");
#endif
	
	if (Thread)
	{
		if (Lock->ExclusiveOwner.OwnerThread == Thread)
			return &Lock->ExclusiveOwner;
	}
	
	PLIST_ENTRY Entry = Lock->SharedOwners.Flink;
	while (Entry != &Lock->SharedOwners)
	{
		PEX_RW_LOCK_OWNER Owner = CONTAINING_RECORD(Entry, EX_RW_LOCK_OWNER, Entry);
		
		if (Owner->OwnerThread == Thread)
			return Owner;
		
		Entry = Entry->Flink;
	}
	
	KeReleaseSpinLock(&Lock->GuardLock, Ipl);
	
	// Add a new one.
	PEX_RW_LOCK_OWNER Owner = MmAllocatePool(POOL_NONPAGED, sizeof(EX_RW_LOCK_OWNER));
	Owner->Locked = 0;
	Owner->OwnerThread = NULL;
	
	KIPL NIpl;
	KeAcquireSpinLock(&Lock->GuardLock, &NIpl);
	ASSERT(NIpl == Ipl);
	
	InsertTailList(&Lock->SharedOwners, &Owner->Entry);
	return Owner;
}

void ExReleaseRwLock(PEX_RW_LOCK Lock)
{
	PKTHREAD CurrentThread = KeGetCurrentThread();
	
	KIPL Ipl;
	KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
	
	bool Exclusive = false;
	PEX_RW_LOCK_OWNER Owner = NULL;
	
	if (Lock->ExclusiveOwner.OwnerThread == CurrentThread)
	{
		Exclusive = true;
		Owner = &Lock->ExclusiveOwner;
	}
	else
	{
		PLIST_ENTRY Entry = Lock->SharedOwners.Flink;
		while (Entry != &Lock->SharedOwners && Owner)
		{
			PEX_RW_LOCK_OWNER TempOwner = CONTAINING_RECORD(Entry, EX_RW_LOCK_OWNER, Entry);
			
			if (TempOwner->OwnerThread == CurrentThread)
				Owner = TempOwner;
			
			Entry = Entry->Flink;
		}
	}
	
#ifdef DEBUG
	if (Owner)
		KeCrash("ExReleaseRwLock: Rwlock not held by thread");
#endif
	
	Owner->Locked--;
	if (Owner->Locked != 0)
	{
		// Was locked recursively
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		return;
	}
	
	// Actually released.
#ifdef DEBUG
	if (Lock->HeldCount == 0)
		KeCrash("ExReleaseRwLock: HeldCount is equal to zero");
#endif
	
	// Remove the owner list item from the list.
	RemoveEntryList(&Owner->Entry);
	MmFreePool(Owner);
	
	Lock->HeldCount--;
	
	if (Lock->HeldCount == 0)
	{
		bool Access = false;
		
		if (!Exclusive)
			// We were holding onto it shared, so try to grant exclusive access.
			Access = true;
		else if (Lock->SharedWaiterCount)
			// We were holding it exclusive, and there are no shared waiters.
			// Have to grant exclusive access.
			Access = true;
		
		if (Access)
		{
			if (Lock->ExclusiveWaiterCount)
			{
				// There is a waiter!  Pass over the lock.
				Lock->ExclusiveWaiterCount--;
				
				PKTHREAD WaitThrd = KeSetEventAndGetWaiter(&Lock->ExclusiveSyncEvent);
				
				Lock->ExclusiveOwner.Locked = 1;
				Lock->ExclusiveOwner.OwnerThread = WaitThrd;
				
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				return;
			}
		}
		
		// Couldn't wake an exclusive waiter, wake up all shared waiters.
		int Waiters = Lock->SharedWaiterCount;
		if (Waiters)
		{
			Lock->SharedWaiterCount = 0;
			Lock->HeldCount = Waiters;
			
			KeReleaseSemaphore(&Lock->SharedSemaphore, Waiters);
			
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			return;
		}
	}
	
	KeReleaseSpinLock(&Lock->GuardLock, Ipl);
	return;
}
