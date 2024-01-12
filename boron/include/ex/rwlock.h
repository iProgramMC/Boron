/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ex/rwlock.h
	
Abstract:
	This header file defines the rwlock struct and its
	action functions.
	
Author:
	iProgramInCpp - 11 January 2024
***/
#pragma once
#include <ke.h>

typedef struct _EX_RW_LOCK_OWNER
{
	int Locked;
	LIST_ENTRY Entry;
	PKTHREAD OwnerThread;
}
EX_RW_LOCK_OWNER, *PEX_RW_LOCK_OWNER;

typedef struct _EX_RW_LOCK
{
	// Lock that guards the structure.
	KSPIN_LOCK GuardLock;
	// An exclusive owner.
	EX_RW_LOCK_OWNER ExclusiveOwner;
	
	EX_RW_LOCK_OWNER SharedOwner;
	
	PEX_RW_LOCK_OWNER OwnerTable;
	
	KSEMAPHORE SharedSemaphore;
	
	KEVENT ExclusiveSyncEvent;
	
	int OwnerTableSize;
	
	int SharedOwnerCount;
	
	int SharedWaiterCount;
	
	int ExclusiveWaiterCount;
	
	int HeldCount;
}
EX_RW_LOCK, *PEX_RW_LOCK;

// Initializes the RW-lock structure.
void ExInitializeRwLock(PEX_RW_LOCK Lock);

// Acquires an RW-lock in exclusive mode.
BSTATUS ExAcquireExclusiveRwLock(PEX_RW_LOCK Lock, bool DontBlock, bool Alertable);

// Acquires an RW-lock in shared mode.
BSTATUS ExAcquireSharedRwLock(PEX_RW_LOCK Lock, bool DontBlock, bool Alertable, bool CanStarve);

// Atomically demotes ownership of the current thread over
// the lock from exclusive to shared.
void ExDemoteToSharedRwLock(PEX_RW_LOCK Lock);

// Releases an owned RW-lock.
void ExReleaseRwLock(PEX_RW_LOCK Lock);
