/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/rwlock.c
	
Abstract:
	This module implements the read-write lock. Acquiring
	it shared means one intends to read from the structure
	guarded by the lock, and acquiring it exclusive means
	one intends to modify the structure.
	
Author:
	iProgramInCpp - 10 January 2024
***/
#include "exp.h"

// NOTE: Thanks to https://github.com/xrarch/mintia/blob/3f33cc5f3b70527fefc029056420c2a46e6afb10/OS/OSKernel/Executive/ExRwLock.df

#define EX_RWLOCK_WAIT_TIMEOUT 300 /* Milliseconds */

#define PASS_OVER_INCREMENT   2 /* Increment when rwlock is passed over to exclusive waiter */
#define PASS_SHARED_INCREMENT 1 /* Increment when rwlock is passed over to shared waiters */

//#define DEBUG_RWLOCK
#ifdef DEBUG_RWLOCK
#define DbgPrintA(...) DbgPrint(__VA_ARGS__)
#else
#define DbgPrintA(...)
#endif

PEX_RW_LOCK_OWNER ExFindOwnerRwLock(PEX_RW_LOCK Lock, PKTHREAD Thread, KIPL Ipl);

BSTATUS ExWaitOnRwLock(PEX_RW_LOCK Lock, void* EventObject);

void ExInitializeRwLock(PEX_RW_LOCK Lock)
{
	KeInitializeSpinLock(&Lock->GuardLock);
	KeInitializeEvent(&Lock->ExclusiveSyncEvent, EVENT_SYNCHRONIZATION, false);
	KeInitializeSemaphore(&Lock->SharedSemaphore, 0, SEMAPHORE_LIMIT_NONE);
	
	memset(&Lock->ExclusiveOwner, 0, sizeof(EX_RW_LOCK_OWNER));
	memset(&Lock->SharedOwner, 0, sizeof(EX_RW_LOCK_OWNER));
	
	Lock->OwnerTable = NULL;
	Lock->OwnerTableSize = 0;
	Lock->SharedOwnerCount = 0;
	Lock->SharedWaiterCount = 0;
	Lock->ExclusiveWaiterCount = 0;
	Lock->HeldCount = 0;
}

void ExDeinitializeRwLock(PEX_RW_LOCK Lock)
{
	if (Lock->OwnerTable)
		MmFreePool(Lock->OwnerTable);
	
	Lock->OwnerTable = NULL;
	Lock->OwnerTableSize = 0;
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
	DbgPrintA("CurrentThread %p acquiring lock exclusive", CurrentThread);
	
	KIPL Ipl;
	KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
	
	if (Lock->HeldCount == 0)
	{
		Lock->HeldCount = 1;
		
		Lock->ExclusiveOwner.Locked = 1;
		Lock->ExclusiveOwner.OwnerThread = CurrentThread;
		
		// Won't initialize the list entry because it's not part of a list.
		
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		DbgPrintA("EXCL(%p): Acquired via heldcount==0 case", CurrentThread);
		return STATUS_SUCCESS;
	}
	
	if (Lock->ExclusiveOwner.Locked != 0)
	{
		if (Lock->ExclusiveOwner.OwnerThread == CurrentThread)
		{
			// Yoink!
			Lock->ExclusiveOwner.Locked += 1;
			
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			DbgPrintA("EXCL(%p): Acquired via exclusive recursive case", CurrentThread);
			return STATUS_SUCCESS;
		}
	}
	else if (DontBlock)
	{
		// HeldCount is not zero, and lock isn't acquired exclusively,
		// return because the lock couldn't be acquired without blocking
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		DbgPrintA("EXCL(%p): Timeout by dontblock", CurrentThread);
		return STATUS_TIMEOUT;
	}
	
	if (Alertable)
	{
		// TODO: Check if we're killed.
	}
	
	Lock->ExclusiveWaiterCount++;
	KeReleaseSpinLock(&Lock->GuardLock, Ipl);
	
	DbgPrintA("EXCL(%p): Waiting on lock", CurrentThread);
	BSTATUS Status = ExWaitOnRwLock(Lock, &Lock->ExclusiveSyncEvent);
	
#ifdef DEBUG
	if (Status)
		KeCrash("ExAcquireExclusiveRwLock: ExWaitOnRwLock failed! %d", Status);
#endif
	
	Lock->ExclusiveOwner.OwnerThread = CurrentThread;
	
	if (Alertable)
	{
		// TODO: Check if the thread was killed and release if so.
	}
	
	DbgPrintA("EXCL(%p): Rwlock acquired, heldcount: %d", CurrentThread, Lock->HeldCount);
	
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
	DbgPrintA("CurrentThread %p acquiring lock shared", CurrentThread);
	
	while (true)
	{
		KIPL Ipl;
		KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
		
		if (Lock->HeldCount == 0)
		{
			// No shared owners, so use the preallocated one.
			Lock->SharedOwner.Locked = true;
			Lock->SharedOwner.OwnerThread = CurrentThread;
			Lock->HeldCount = 1;
			
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			DbgPrintA("SHRD(%p): Acquired via heldcount==0 case", CurrentThread);
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
				DbgPrintA("SHRD(%p): Acquired via rescursive locking case", CurrentThread);
				return STATUS_SUCCESS;
			}
			
			Owner = ExFindOwnerRwLock(Lock, NULL, Ipl);
			
			if (!Owner)
			{
				// Try again!
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				DbgPrintA("SHRD(%p): Release spinlock because owner not found", CurrentThread);
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
				DbgPrintA("SHRD(%p): Release spinlock because owner not found 2", CurrentThread);
				continue;
			}
			
			if (CurrentThread == Owner->OwnerThread)
			{
				// Yoink recursively?
				// This is probably not needed but do it anyway
				Owner->Locked++;
				
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				DbgPrintA("SHRD(%p): Acquired via recursively locking case", CurrentThread);
				return STATUS_SUCCESS;
			}
			
			DbgPrintA("SHRD(%p): Exclusive waiters: %d", CurrentThread, Lock->ExclusiveWaiterCount);
			if (CanStarve || Lock->ExclusiveWaiterCount == 0)
			{
				// Yoink!
				Owner->OwnerThread = CurrentThread;
				Owner->Locked = 1;
				Lock->HeldCount++;
				
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				DbgPrintA("SHRD(%p): Acquired via exclusivewaitercount==0 case", CurrentThread);
				return STATUS_SUCCESS;
			}
			else if (DontBlock)
			{
				KeReleaseSpinLock(&Lock->GuardLock, Ipl);
				DbgPrintA("SHRD(%p): Timeout in dontblock", CurrentThread);
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
		DbgPrintA("SHRD(%p): Releasing spinlock because about to wait", CurrentThread);
		break;
	}
	
	// Wait!
	DbgPrintA("SHRD(%p): Waiting on lock", CurrentThread);
	BSTATUS Status = ExWaitOnRwLock(Lock, &Lock->SharedSemaphore);
	
#ifdef DEBUG
	if (Status)
		KeCrash("ExAcquireSharedRwLock: ExWaitOnRwLock failed! %d", Status);
#endif
	
	if (Alertable)
	{
		// TODO: Check if the thread was killed and release if so.
	}
	
	DbgPrintA("SHRD(%p): Rwlock acquired, heldcount: %d", CurrentThread, Lock->HeldCount);
	
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
			
			KeReleaseSemaphore(&Lock->SharedSemaphore, Waiters, PASS_SHARED_INCREMENT);
		}
		
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		break;
	}
}

BSTATUS ExWaitOnRwLock(UNUSED PEX_RW_LOCK Lock, void* EventObject)
{
#ifdef DEBUG
	if (Lock->GuardLock.Locked && KeGetProcessorCount() == 1)
		KeCrash("ExWaitOnRwLock: Rwlock's spinlock is locked for some reason");
#endif

	while (true)
	{
		BSTATUS Status = KeWaitForSingleObject(EventObject, false, EX_RWLOCK_WAIT_TIMEOUT, MODE_KERNEL);
		
		if (Status != STATUS_TIMEOUT)
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
	
	int OldSize = Lock->OwnerTableSize;
	PEX_RW_LOCK_OWNER Table = Lock->OwnerTable;
	
	PEX_RW_LOCK_OWNER FreeEntry = NULL;
	
	if (Lock->SharedOwner.OwnerThread == NULL)
		FreeEntry = &Lock->SharedOwner;
	
	for (int i = 0; i < OldSize; i++)
	{
		if (Table[i].OwnerThread == Thread)
			return &Table[i];
		
		if (!FreeEntry && Table[i].OwnerThread == NULL)
			FreeEntry = &Table[i];
	}
	
	// Didn't find a pre-existing entry.
	if (FreeEntry)
		return FreeEntry;
	
	// Need to expand the table.
	int NewSize = OldSize + 4;
	
	// TODO: This allocation shouldn't fail.  Let the allocator
	// know about that directly instead of forcing.
	PEX_RW_LOCK_OWNER NewTable;
	
	do
	{
		KeReleaseSpinLock(&Lock->GuardLock, Ipl);
		
		NewTable = MmAllocatePool(POOL_NONPAGED, sizeof(EX_RW_LOCK_OWNER) * NewSize);
		
		KeAcquireSpinLock(&Lock->GuardLock, &Ipl);
		
		// If someone expanded it already:
		if (Lock->OwnerTableSize > OldSize)
		{
			// Free what we did, if we did anything, and retry.
			if (NewTable)
				MmFreePool(NewTable);
			
			NewTable = NULL;
		}
	}
	while (!NewTable);
	
	// Allocated something, and no one expanded it already.
	memcpy(NewTable, Lock->OwnerTable, sizeof(EX_RW_LOCK_OWNER) * OldSize);
	memset(NewTable + OldSize, 0, sizeof(EX_RW_LOCK_OWNER) * (NewSize - OldSize));
	
	FreeEntry = &NewTable[OldSize];
	
	if (Lock->OwnerTable)
		MmFreePool(Lock->OwnerTable);
	
	Lock->OwnerTable = NewTable;
	Lock->OwnerTableSize = NewSize;
	
	return FreeEntry;
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
	else if (Lock->SharedOwner.OwnerThread == CurrentThread)
	{
		Exclusive = false;
		Owner = &Lock->SharedOwner;
	}
	else
	{
		Exclusive = false;
		
		PEX_RW_LOCK_OWNER Table = Lock->OwnerTable;
		int TableSize = Lock->OwnerTableSize;
		for (int i = 0; i < TableSize; i++)
		{
			if (Table[i].OwnerThread == CurrentThread)
			{
				Owner = &Table[i];
				break;
			}
		}
	}
	
#ifdef DEBUG
	if (!Owner)
		KeCrash("ExReleaseRwLock: Rwlock not held by thread %p", CurrentThread);
	if (Owner->Locked == 0)
		KeCrash("ExReleaseRwLock: Rwlock not held by thread %p (?!?)", CurrentThread);
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
		KeCrash("ExReleaseRwLock: HeldCount is equal to zero. Thread %p", CurrentThread);
#endif
	
	Owner->OwnerThread = NULL;
	
	Lock->HeldCount--;
	
	if (Lock->HeldCount == 0)
	{
		bool Access = false;
		
		if (!Exclusive)
		{
			// We were holding onto it shared, so try to grant exclusive access.
			Access = true;
		}
		else if (!Lock->SharedWaiterCount)
		{
			// We were holding it exclusive, and there are no shared waiters.
			// Have to grant exclusive access.
			Access = true;
		}
		
		if (Access)
		{
			if (Lock->ExclusiveWaiterCount)
			{
				// There is a waiter!  Pass over the lock.
				Lock->HeldCount = 1;
				Lock->ExclusiveWaiterCount--;
				
				PKTHREAD WaitThrd = KeSetEventAndGetWaiter(&Lock->ExclusiveSyncEvent, PASS_OVER_INCREMENT);
				
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
			
			KeReleaseSemaphore(&Lock->SharedSemaphore, Waiters, PASS_SHARED_INCREMENT);
			
			KeReleaseSpinLock(&Lock->GuardLock, Ipl);
			return;
		}
	}
	
	KeReleaseSpinLock(&Lock->GuardLock, Ipl);
	return;
}
