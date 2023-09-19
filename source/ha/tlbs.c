/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ha/tlbs.c
	
Abstract:
	This module contains the AMD64 platform's specific
	TLB shootdown routine.
	
Author:
	iProgramInCpp - 16 September 2023
***/
#include <hal.h>
#include <ke.h>
#include "apic.h"

extern CPU** KeProcessorList;
extern int   KeProcessorCount;

// only one thread can perform a TLB shootdown at once.. don't exactly know why
// the locks inside the CPUs themselves are used for synchronization of the operation itself
SpinLock HalTLBSLock;

void HalIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	KeLock(&HalTLBSLock);
	
	// Invalidate the pages on the local CPU
	for (size_t i = 0; i < Length; i++)
	{
		KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
	}
	
	// If we are the only processor, return
	if (KeProcessorCount == 1)
	{
		KeUnlock(&HalTLBSLock);
		return;
	}
	
	int OwnId = KeGetCPU()->Id;
	
	for (int i = 0; i < KeProcessorCount; i++)
	{
		// lock the TLB shootdown lock for the first time
		KeLock(&KeProcessorList[i]->TlbsLock);
		
		// write the address and length
		KeProcessorList[i]->TlbsAddress = Address;
		KeProcessorList[i]->TlbsLength  = Length;
	}
	
	// OK! Now that all CPUs are ready for the TLB shootdown, it shall commence:
	HalBroadcastIpi(INTV_TLBS_IPI, false);
	
	// Done, now make sure all cores did it with a short lock-unlock cycle
	for (int i = 0; i < KeProcessorCount; i++)
	{
		// lock the TLB shootdown lock for the first time
		if (i != OwnId)
			KeLock(&KeProcessorList[i]->TlbsLock);
		
		KeUnlock(&KeProcessorList[i]->TlbsLock);
	}
	
	KeUnlock(&HalTLBSLock);
}
