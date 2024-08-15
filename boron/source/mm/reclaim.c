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
		PMMPTE PtePtr = MiGetPTEPointer(MiGetCurrentPageMap(), i, false);
		ASSERT(PtePtr);
		
		MMPTE Pte = *PtePtr;
		ASSERT(Pte);
		
		// Clear the PTE without a TLB shootdown.  It's not necessary because
		// we're purging a read-only section.
		*PtePtr = 0;
		
		MMPFN Pfn = (MMPFN)((Pte & MM_PTE_ADDRESSMASK) >> 12);
		MmFreePhysicalPage(Pfn);
		Reclaimed++;
	}
	MmUnlockKernelSpace();
	
	DbgPrint("Reclaimed %d pages from init.", Reclaimed);
}
