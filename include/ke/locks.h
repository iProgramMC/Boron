/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/locks.h
	
Abstract:
	This header file contains the definitions for the
	locking primitives used in the kernel.
	
Author:
	iProgramInCpp - 28 September 2023
***/
#ifndef BORON_KE_LOCKS_H
#define BORON_KE_LOCKS_H

// simple spin locks, for when contention is rare
typedef struct
{
	bool Locked;
}
KSPIN_LOCK, *PKSPIN_LOCK;

void KeInitializeSpinLock(PKSPIN_LOCK);
void KeAcquireSpinLock(PKSPIN_LOCK);
void KeReleaseSpinLock(PKSPIN_LOCK);
bool KeAttemptAcquireSpinLock(PKSPIN_LOCK);

// ticket locks, for when contention is definitely possible
typedef struct
{
	int NowServing;
	int NextNumber;
}
KTICKET_LOCK, *PKTICKET_LOCK;

void KeInitializeTicketLock(PKTICKET_LOCK);
void KeAcquireTicketLock(PKTICKET_LOCK);
void KeReleaseTicketLock(PKTICKET_LOCK);

#endif//BORON_KE_LOCKS_H
