/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/init.c
	
Abstract:
	This module implements the code that reclaims the .text.init
	section of the kernel for re-use.
	
Author:
	iProgramInCpp - 15 August 2024
***/
#include "mi.h"

extern char KiTextInitStart[];
extern char KiTextInitEnd[];

void MiReclaimInitText()
{
	int Reclaimed = 0;
	
	MmLockKernelSpaceExclusive();
	for (uintptr_t i = (uintptr_t) KiTextInitStart; i != (uintptr_t) KiTextInitEnd; i += PAGE_SIZE)
	{
		PMMPTE PtePtr = MmGetPteLocationCheck(i, false);
		ASSERT(PtePtr);
		
		MMPTE Pte = *PtePtr;
		ASSERT(Pte);
		
		// Clear the PTE.  A TLB shootdown is normally not necessary because
		// the memory region will never be read from again. However, in debug
		// mode, a TLB shootdown will be performed anyway.
		*PtePtr = 0;
		
		MMPFN Pfn = MM_PTE_PFN(Pte);
		MmFreePhysicalPage(Pfn);
		Reclaimed++;
	}
	
	MmUnlockKernelSpace();
	
#ifdef DEBUG
	//MmIssueTLBShootDown((uintptr_t) KiTextInitStart, ((uintptr_t)KiTextInitEnd - (uintptr_t)KiTextInitStart + PAGE_SIZE - 1) / PAGE_SIZE);
#endif
	
	DbgPrint("Reclaimed %d pages from init.", Reclaimed);
}
