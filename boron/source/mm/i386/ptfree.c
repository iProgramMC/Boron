/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/i386/ptfree.c
	
Abstract:
	This module implements the function that frees unused
	page table mapping levels.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#include "../mi.h"

#define PTES_PER_LEVEL         (PAGE_SIZE / sizeof(MMPTE)) // 1024
#define PTES_COVERED_BY_ONE_PT (PTES_PER_LEVEL * PTES_PER_LEVEL) // 1048576

// Gets an address down the tree.
//
// For example, if you're in the PML2, this will give you
// the relevant PML1 address.
PMMPTE MiGetSubPteAddress(PMMPTE PteAddress)
{
	MMADDRESS_CONVERT Address;
	Address.Long = (uintptr_t) PteAddress;
	
	Address.Level2Index = Address.Level1Index;
	Address.Level1Index = Address.PageOffset / sizeof(PMMPTE);
	Address.PageOffset = 0;
	
	return (PMMPTE) Address.Long;
}

static bool MmpIsPteListCompletelyEmpty(PMMPTE Pte)
{
	bool AllZeroes = true;
	
	for (size_t PteIndex = 0; PteIndex < PTES_PER_LEVEL; PteIndex++)
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
	
	MMADDRESS_CONVERT Address;
	PMMPTE Pte;
	
	Address.Long = StartVa;
	Pte = (PMMPTE) MI_PML2_LOCATION + Address.Level2Index;
	
	// Scan each page table in the range.
	for (size_t i = 0; i < SizePages; i += PTES_COVERED_BY_ONE_PT, ++Pte)
	{
		if (~(*Pte) & MM_PTE_PRESENT)
			continue;
		
		PMMPTE SubPte = MiGetSubPteAddress(Pte);
		if (MmpIsPteListCompletelyEmpty(SubPte))
		{
			MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
			*Pte = 0;
		}
	}
}
