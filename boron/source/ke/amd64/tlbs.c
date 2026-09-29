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

#ifndef DEBUG2
#define TestDbgPrint(...)
#else
#define TestDbgPrint DbgPrint
#endif

#define MAX_TLBS_LENGTH 32

extern PKPRCB* KeProcessorList;
extern int     KeProcessorCount;

// only one thread can perform a TLB shootdown at once.. don't exactly know why
// the locks inside the CPUs themselves are used for synchronization of the operation itself
KSPIN_LOCK KeTLBSLock;

void KeIssueTLBShootDown(uintptr_t Address, size_t LengthPages, PKPROCESS Process)
{
	if (LengthPages == 0)
		LengthPages = 1;
	
	// TODO: Is it a good idea to allow dynamic memory allocation before SMP init?
	// I'll allow it for now...
	//
	// The idea is, if the processor list wasn't setup and we don't have a current PRCB,
	// chances are that the other CPUs aren't active, therefore just invalidate the pages
	// here and return.
	if (!KeGetCurrentPRCB())
	{
		// Invalidate the pages on the local CPU
		
		if (LengthPages >= MAX_TLBS_LENGTH)
		{
			KeSetCurrentPageTable(KeGetCurrentPageTable());
		}
		else
		{
			for (size_t i = 0; i < LengthPages; i++)
				KeInvalidatePage((void*)(Address + i * PAGE_SIZE));
		}
		
		return;
	}
	
	TestDbgPrint("TLBS: Start (%p, %zu, %p) (Caller: %p, %p)", Address, LengthPages, Process, __builtin_return_address(0), __builtin_return_address(1));
	
	KIPL OldIpl, UnusedIpl;
	KIPL CurrentIpl = KeGetIPL();
	KeAcquireSpinLock(&KeTLBSLock, &OldIpl);
	
	// The bitmap of processors to request a shootdown for.
	// If a process is specified, use the bitmap of active
	// threads instead.
	const uint64_t ShootdownBitmapAll = 0xFFFFFFFFFFFFFFFF;
	uint64_t ShootdownBitmap = ShootdownBitmapAll;
	
	if (Process)
	{
		// TODO: release this code - need to test that ActiveAPBitmap works first
		ShootdownBitmap = Process->ActiveAPBitmap;
		
	#ifdef DEBUG
		uint64_t Bit = 1ULL << KeGetCurrentPRCB()->Id;
		if (~ShootdownBitmap & Bit) {
			KeCrash("KeIssueTLBShootDown: Current process' ActiveAPBitmap doesn't have our own bit set.  Impossible!");
		}
	#endif
	}
	
	TestDbgPrint("	TLBS: Bitmap: %016llx", ShootdownBitmap);
	TestDbgPrint("	TLBS: We're processor #%d", KeGetCurrentPRCB()->Id);
	
#ifdef DEBUG
	// If we have the "track spinlock counts" feature active, we should back up the spinlock
	// count here and restore it later.
	//
	// This is because we use spinlocks in a "strange" way to synchronize the TLB shootdown
	// process.  Basically, each processor's TLB shootdown lock is acquired once.
	// Then, it's acquired again. But since it was already acquired once, this will
	// initiate a wait.  Luckily, the other threads will receive the TLB shootdown interrupt,
	// perform the TLB invalidations, and then 
	int SpinlocksHeld = KeGetCurrentThread()->HoldingSpinlocks;
#endif
	
	// Invalidate the pages on the local CPU
	for (size_t i = 0; i < LengthPages; i++)
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
		if (!((ShootdownBitmap >> i) & 1))
			continue;
		
		// lock the TLB shootdown lock for the first time
		TestDbgPrint("	TLBS: Acquiring TLBS lock for CPU %d", i);
		KeAcquireSpinLock(&KeProcessorList[i]->TlbsLock, &UnusedIpl);
		
		// write the address and length
		KeProcessorList[i]->TlbsAddress = Address;
		KeProcessorList[i]->TlbsLength  = LengthPages;
	}
	
	// OK! Now that all CPUs are ready for the TLB shootdown, it shall commence:
	if (ShootdownBitmap == ShootdownBitmapAll)
	{
		TestDbgPrint("	TLBS: Requesting IPI for ALL processors");
		HalRequestIpi(0, HAL_IPI_BROADCAST, KiVectorTlbShootdown);
	}
	else
	{
		for (int i = 0; i < KeProcessorCount; i++)
		{
			if (!((ShootdownBitmap >> i) & 1))
				continue;
			if (i == OwnId)
				continue;
			
			PKPRCB Prcb = KeProcessorList[i];
			TestDbgPrint("	TLBS: Requesting IPI for CPU %d (lapic id %u)", i, Prcb->LapicId);
			HalRequestIpi(Prcb->LapicId, 0, KiVectorTlbShootdown);
		}
	}
	
	// Done, now make sure all cores did it with a short lock-unlock cycle
	for (int i = 0; i < KeProcessorCount; i++)
	{
		if (!((ShootdownBitmap >> i) & 1))
			continue;
		
		// lock the TLB shootdown lock for the first time
		if (i != OwnId) {
			TestDbgPrint("	TLBS: Acquiring TLBS lock for CPU %d for finish", i);
			KeAcquireSpinLock(&KeProcessorList[i]->TlbsLock, &UnusedIpl);
		}
		
		TestDbgPrint("	TLBS: Releasing TLBS lock for CPU %d", i);
		KeReleaseSpinLock(&KeProcessorList[i]->TlbsLock, CurrentIpl);
	}
	
#ifdef DEBUG
	KeGetCurrentThread()->HoldingSpinlocks = SpinlocksHeld;
#endif
	
	KeReleaseSpinLock(&KeTLBSLock, OldIpl);
}

PKREGISTERS KiHandleTlbShootdownIpi(PKREGISTERS Regs)
{
	PKPRCB Prcb = KeGetCurrentPRCB();
	
	TestDbgPrint("	TLBS: Handling TLB shootdown on CPU %u", Prcb->LapicId);
	
	if (Prcb->TlbsLength >= MAX_TLBS_LENGTH)
	{
		KeSetCurrentPageTable(KeGetCurrentPageTable());
	}
	else
	{
		for (size_t i = 0; i < Prcb->TlbsLength; i++)
			KeInvalidatePage((void*)(Prcb->TlbsAddress + i * PAGE_SIZE));
	}
	
#ifdef DEBUG
	// The spinlock was already acquired by the CPU that initiated
	// the TLB shootdown.  All we need to do is release it.  But we
	// should also let the spinlocks held tracker know.
	if (Prcb->Scheduler.CurrentThread)
		Prcb->Scheduler.CurrentThread->HoldingSpinlocks++;
#endif
	
	KeReleaseSpinLock(&Prcb->TlbsLock, IPL_NOINTS);
	
	HalEndOfInterrupt((int) Regs->IntNumber);
	return Regs;
}
