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
#include <string.h>

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
	char buffer[64];
	//snprintf(buffer,sizeof buffer,"KeAcquireSpinLock(%p)\n",SpinLock);
	//HalDisplayString2(buffer);
	//snprintf(buffer,sizeof buffer,"PROCESSOR COUNT = %d\n", KeGetProcessorCount());
	//HalDisplayString2(buffer);
	
	*OldIpl = KeRaiseIPLIfNeeded(IPL_DPC);
	
	//snprintf(buffer,sizeof buffer,"Raised IPL to DPC\n");
	//HalDisplayString2(buffer);
#ifdef DEBUG
	// If we are a uniprocessor system, check if the lock is locked.
	// If it is already locked, it can only mean one thing...
	// ... a deadlock has occurred!!
	if (KeGetProcessorCount() == 1 && SpinLock->Locked)
	{
		HalDisplayString2("About to crash...\n");
#ifdef SPINLOCK_TRACK_PC
	#ifdef IS_64_BIT
		KeCrash("KeAcquireSpinLock: spinlock already locked by %p", SpinLock->Pc | 0xFFFF000000000000);
	#else
		KeCrash("KeAcquireSpinLock: spinlock already locked by %p", SpinLock->Pc);
	#endif
#else
		KeCrash("KeAcquireSpinLock: spinlock already locked");
#endif
	}
		
#endif

	//HalDisplayString2("Entering while loop....!\n");
	
	while (true)
	{
		snprintf(buffer, sizeof buffer, "Doing the atomic CAS....   %d!\n", sizeof(SpinLock->Locked));
		HalDisplayString2(buffer);
		if (!AtTestAndSetMO(SpinLock->Locked, ATOMIC_MEMORD_ACQUIRE))
		{
			//HalDisplayString2("Entering the spinlock....!\n");
			//snprintf(buffer, sizeof buffer, "The spinlock %p is now unlocked.\n", SpinLock);
			//HalDisplayString2(buffer);
			
#ifdef DEBUG
#ifdef SPINLOCK_TRACK_PC
			SpinLock->Pc = CallerAddress();
#endif
			
			if (KeGetCurrentPRCB())
			{
				PKTHREAD CurrThread = KeGetCurrentThread();
				if (CurrThread)
					CurrThread->HoldingSpinlocks++;
			}
#endif
			return;
		}
		
		// Use regular reads instead of atomic reads to minimize bus contention
		//HalDisplayString2("Entering a loop that constantly checks that the spinlock is locked....!\n");
		//snprintf(buffer, sizeof buffer, "The Spinlock %p is locked by %p\n", SpinLock, SpinLock->Pc);
		//HalDisplayString2(buffer);
		while (SpinLock->Locked)
		{
			//snprintf(buffer, sizeof buffer, "Spinlock %p  locked by %p\n", SpinLock, SpinLock->Pc);
			//HalDisplayString2(buffer);
			KeSpinningHint();
		}
		
		//HalDisplayString2("Continuing with loop....!\n");
	}
}

void KeReleaseSpinLock(PKSPIN_LOCK SpinLock, KIPL OldIpl)
{
#ifdef DEBUG
	// If we are a uniprocessor system, check if the lock is locked.
	// If it is already locked, it can only mean one thing...
	// ... a deadlock has occurred!!
	if (!SpinLock->Locked)
		KeCrash("KeReleaseSpinLock: spinlock was not acquired");
	
#ifdef SPINLOCK_TRACK_PC
	SpinLock->Pc = -1;
#endif
	
	if (KeGetCurrentPRCB())
	{
		PKTHREAD CurrThread = KeGetCurrentThread();
		if (CurrThread)
			CurrThread->HoldingSpinlocks--;
	}
#endif

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
