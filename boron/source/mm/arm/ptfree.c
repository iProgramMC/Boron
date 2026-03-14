/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/armv6/ptfree.c
	
Abstract:
	This module implements the function that frees unused
	page table mapping levels.
	
Author:
	iProgramInCpp - 30 December 2025
***/
#include "../mi.h"

static bool MmpIsPteListCompletelyEmpty(PMMPTE Pte)
{
	bool AllZeroes = true;
	
	for (size_t PteIndex = 0; PteIndex < 1024; PteIndex++)
	{
		if (Pte[PteIndex] != 0)
		{
			AllZeroes = false;
			break;
		}
	}
	
	return AllZeroes;
}

// NOTE: StartVa and SizePages are only roughly followed.
//
// NOTE: The address space lock of the process *must* be held.
void MiFreeUnusedMappingLevelsInCurrentMap(uintptr_t StartVa, size_t SizePages)
{
	if (StartVa >= MM_KERNEL_SPACE_BASE)
		return;
	
	PMMPTE Pml1 = (PMMPTE) MI_PML1_LOCATION;
	PMMPTE Pml2 = (PMMPTE) MI_PML1_LOCATION;
	PMMPTE Pte = &Pml1[(StartVa >> 20) & ~3];
	
	for (size_t i = 0; i < SizePages; i += 1024, Pte += 4)
	{
		if (!*Pte)
			continue;
		
		PMMPTE SubPte = &Pml2[(StartVa >> 12)];
		if (MmpIsPteListCompletelyEmpty(SubPte))
		{
			MMPFN Pfn = (*Pte >> 12);
			MmFreePhysicalPage(Pfn);
			
			for (int m = 0; m < 4; m++) {
				Pte[m] = 0;
			}
			
			// also clear the pml2 mirror:
			PMMPTE Pml2Mirror = (PMMPTE) MI_PML2_MIRROR_LOCATION;
			Pml2Mirror[(StartVa >> 22)] = 0;
		}
	}
	
	KeFlushTLB();
}
