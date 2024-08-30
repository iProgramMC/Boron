/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/amd64/tlbs.c
	
Abstract:
	This module contains the AMD64 platform's specific
	TLB shootdown routine.
	
Author:
	iProgramInCpp - 27 October 2023
***/
#include <ke.h>
#include <hal.h>
#include "archi.h"

extern PKPRCB* KeProcessorList;
extern int     KeProcessorCount;

// only one thread can perform a TLB shootdown at once.. don't exactly know why
// the locks inside the CPUs themselves are used for synchronization of the operation itself
KSPIN_LOCK KeTLBSLock;

void KeIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	// TODO: Is it a good idea to allow dynamic memory allocation before SMP init?
	// I'll allow it for now...
	//
	// The idea is, if the processor list wasn't setup and we don't have a current PRCB,
	// chances are that the other CPUs aren't active, therefore just invalidate the pages
	// here and return.
	if (!KeGetCurrentPRCB())
	{
		// Invalidate the pages on the local CPU
		for (size_t i = 0; i < Length; i++)
		{
			KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
		}
		
		return;
	}
	
	KIPL OldIpl, UnusedIpl;
	KIPL CurrentIpl = KeGetIPL();
	KeAcquireSpinLock(&KeTLBSLock, &OldIpl);
	
	// Invalidate the pages on the local CPU
	for (size_t i = 0; i < Length; i++)
	{
		KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
	}
	
	// If we are the only processor, return
	if (KeProcessorCount == 1)
	{
		KeReleaseSpinLock(&KeTLBSLock, OldIpl);
		return;
	}
	
	int OwnId = KeGetCurrentPRCB()->Id;
	
	for (int i = 0; i < KeProcessorCount; i++)
	{
		// lock the TLB shootdown lock for the first time
		KeAcquireSpinLock(&KeProcessorList[i]->TlbsLock, &UnusedIpl);
		
		// write the address and length
		KeProcessorList[i]->TlbsAddress = Address;
		KeProcessorList[i]->TlbsLength  = Length;
	}
	
	// OK! Now that all CPUs are ready for the TLB shootdown, it shall commence:
	HalRequestIpi(0, HAL_IPI_BROADCAST, KiVectorTlbShootdown);
	
	// Done, now make sure all cores did it with a short lock-unlock cycle
	for (int i = 0; i < KeProcessorCount; i++)
	{
		// lock the TLB shootdown lock for the first time
		if (i != OwnId)
			KeAcquireSpinLock(&KeProcessorList[i]->TlbsLock, &UnusedIpl);
		
		KeReleaseSpinLock(&KeProcessorList[i]->TlbsLock, CurrentIpl);
	}
	
	KeReleaseSpinLock(&KeTLBSLock, OldIpl);
}

PKREGISTERS KiHandleTlbShootdownIpi(PKREGISTERS Regs)
{
	PKPRCB Prcb = KeGetCurrentPRCB();
	
#ifdef DEBUG2
	DbgPrint("Handling TLB shootdown on CPU %u", Prcb->LapicId);
#endif
	
	for (size_t i = 0; i < Prcb->TlbsLength; i++)
	{
		KeInvalidatePage((void*)(Prcb->TlbsAddress + i * PAGE_SIZE));
	}
	
	KeReleaseSpinLock(&Prcb->TlbsLock, IPL_NOINTS);
	
	HalEndOfInterrupt();
	return Regs;
}
