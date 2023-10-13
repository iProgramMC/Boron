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

// Note! The reason we raise IPL when locking a spinlock is that we don't want
// the scheduler to interrupt the critical section, thus wasting precious cycles
// on other processors waiting for the spin lock.

void KeInitializeSpinLock(PKSPIN_LOCK SpinLock)
{
	SpinLock->Locked = 0;
}

bool KeAttemptAcquireSpinLock(PKSPIN_LOCK SpinLock, PKIPL OldIpl)
{
	*OldIpl = KeRaiseIPLIfNeeded(IPL_DPC);
	
	if (!AtTestAndSetMO(SpinLock->Locked, ATOMIC_MEMORD_ACQUIRE))
		return true;
	
	KeLowerIPL(*OldIpl);
	return false;
}

void KeAcquireSpinLock(PKSPIN_LOCK SpinLock, PKIPL OldIpl)
{
	*OldIpl = KeRaiseIPLIfNeeded(IPL_DPC);
	
	while (true)
	{
		if (!AtTestAndSetMO(SpinLock->Locked, ATOMIC_MEMORD_ACQUIRE))
			return;
		
		// Use regular reads instead of atomic reads to minimize bus contention
		while (SpinLock->Locked)
			KeSpinningHint();
	}
}

void KeReleaseSpinLock(PKSPIN_LOCK SpinLock, KIPL OldIpl)
{
	AtClearMO(SpinLock->Locked, ATOMIC_MEMORD_RELEASE);
	KeLowerIPL(OldIpl);
}

void KeInitializeTicketLock(PKTICKET_LOCK TicketLock)
{
	AtClear(TicketLock->NowServing);
	AtClear(TicketLock->NextNumber);
}

void KeAcquireTicketLock(PKTICKET_LOCK TicketLock, PKIPL OldIpl)
{
	int MyTicket = AtFetchAddMO(TicketLock->NextNumber, 1, ATOMIC_MEMORD_ACQUIRE);
	*OldIpl = KeRaiseIPLIfNeeded(IPL_DPC);
	
	while (AtLoadMO(TicketLock->NowServing, ATOMIC_MEMORD_ACQUIRE) != MyTicket)
		KeSpinningHint();
}

void KeReleaseTicketLock(PKTICKET_LOCK TicketLock, KIPL OldIpl)
{
	AtAddFetchMO(TicketLock->NowServing, 1, ATOMIC_MEMORD_RELEASE);
	KeLowerIPL(OldIpl);
}
