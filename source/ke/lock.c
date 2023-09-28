/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/lock.c
	
Abstract:
	This module implements the spin lock and ticket lock, two
	basic locking primitives.
	
Author:
	iProgramInCpp - 20 August 2023
***/

#include <ke.h>
#include <arch.h>

void KeInitializeSpinLock(PKSPIN_LOCK SpinLock)
{
	SpinLock->Locked = 0;
}

bool KeAttemptAcquireSpinLock(PKSPIN_LOCK SpinLock)
{
	return !AtTestAndSetMO(SpinLock->Locked, ATOMIC_MEMORD_ACQUIRE);
}

void KeAcquireSpinLock(PKSPIN_LOCK SpinLock)
{
	while (true)
	{
		if (KeAttemptAcquireSpinLock(SpinLock))
			return;
		
		while (AtLoadMO(SpinLock->Locked, ATOMIC_MEMORD_ACQUIRE))
			KeSpinningHint();
	}
}

void KeReleaseSpinLock(PKSPIN_LOCK SpinLock)
{
	AtClearMO(SpinLock->Locked, ATOMIC_MEMORD_RELEASE);
}

void KeInitializeTicketLock(PKTICKET_LOCK TicketLock)
{
	AtClear(TicketLock->NowServing);
	AtClear(TicketLock->NextNumber);
}

void KeAcquireTicketLock(PKTICKET_LOCK TicketLock)
{
	int MyTicket = AtFetchAddMO(TicketLock->NextNumber, 1, ATOMIC_MEMORD_ACQUIRE);
	
	while (AtLoadMO(TicketLock->NowServing, ATOMIC_MEMORD_ACQUIRE) != MyTicket)
		KeSpinningHint();
}

void KeReleaseTicketLock(PKTICKET_LOCK TicketLock)
{
	AtAddFetchMO(TicketLock->NowServing, 1, ATOMIC_MEMORD_RELEASE);
}
