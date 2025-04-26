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

#include <ke/ipl.h>

// simple spin locks, for when contention is rare
typedef struct
{
	bool Locked;
#if defined(DEBUG) && defined(SPINLOCK_TRACK_PC)
	uint64_t Pc : 48; // Locking program counter - debug only
#endif
}
KSPIN_LOCK, *PKSPIN_LOCK;

void KeInitializeSpinLock(PKSPIN_LOCK);
void KeAcquireSpinLock(PKSPIN_LOCK, PKIPL OldIpl);
void KeReleaseSpinLock(PKSPIN_LOCK, KIPL OldIpl);
bool KeAttemptAcquireSpinLock(PKSPIN_LOCK, PKIPL OldIpl);

// ticket locks, for when contention is definitely possible
typedef struct
{
	int NowServing;
	int NextNumber;
}
KTICKET_LOCK, *PKTICKET_LOCK;

void KeInitializeTicketLock(PKTICKET_LOCK);
void KeAcquireTicketLock(PKTICKET_LOCK, PKIPL OldIpl);
void KeReleaseTicketLock(PKTICKET_LOCK, KIPL OldIpl);

#endif//BORON_KE_LOCKS_H
