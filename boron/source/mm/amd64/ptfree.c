/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/amd64/ptfree.c
	
Abstract:
	This module implements the function that frees unused
	page table mapping levels.
	
Author:
	iProgramInCpp - 14 March 2025
***/
#include "../mi.h"

#define PTES_PER_LEVEL       (PAGE_SIZE / sizeof(MMPTE)) // 512
#define PAGE_MAP_LEVELS       4
#define PTES_COVERED_BY_PML4  PTES_PER_LEVEL * PTES_PER_LEVEL * PTES_PER_LEVEL

// Gets an address down the tree.
//
// For example, if you're in the PML4, this will give you
// the relevant PML3 address.
PMMPTE MiGetSubPteAddress(PMMPTE PteAddress)
{
	MMADDRESS_CONVERT Address;
	Address.Long = (uintptr_t) PteAddress;
	
	Address.Level4Index = Address.Level3Index;
	Address.Level3Index = Address.Level2Index;
	Address.Level2Index = Address.Level1Index;
	Address.Level1Index = Address.PageOffset / sizeof(PMMPTE);
	Address.PageOffset = 0;
	Address.SignExtension = 0xFFFF;
	
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

static bool MmpFreeUnusedMappingLevelsInCurrentMapPML(PMMPTE Pte, int RecursionCount);

// NOTE: StartVa and SizePages are only roughly followed.
//
// NOTE: The address space lock of the process *must* be held.
// This also issues a TLB shootdown covering the affected area.
void MiFreeUnusedMappingLevelsInCurrentMap(uintptr_t StartVa, size_t SizePages)
{
	bool ShouldUnmapPML4;
	MMADDRESS_CONVERT Address;
	PMMPTE Pte;
	
	ShouldUnmapPML4 = true;
	Address.Long = StartVa;
	
	Pte = (PMMPTE) MI_PML3_LOCATION + Address.Level4Index;
	
	// Currently, we can only operate in user space.  This is because
	// in kernel space, the PML4 pages may never be deallocated. 
	if (Address.Level4Index >= PTES_PER_LEVEL / 2)
		ShouldUnmapPML4 = false;
	
	for (size_t PageNumber = 0;
	     PageNumber < SizePages;
	     PageNumber += PTES_COVERED_BY_PML4,
	     ++Pte)
	{
		if (!MM_PTE_ISPRESENT(*Pte))
			continue;
		
		PMMPTE SubPte = MiGetSubPteAddress(Pte);
		if (MmpIsPteListCompletelyEmpty(SubPte) && ShouldUnmapPML4)
		{
			MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
			*Pte = 0;
			continue;
		}
		
		if (!MmpFreeUnusedMappingLevelsInCurrentMapPML(SubPte, PAGE_MAP_LEVELS - 1))
			continue;
		
		// Returned true, so this is now ready to free.
		if (ShouldUnmapPML4)
		{
			MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
			*Pte = 0;
		}
	}
}

static bool MmpFreeUnusedMappingLevelsInCurrentMapPML(PMMPTE Pte, int MapLevel)
{
	if (MapLevel <= 1)
	{
		// We're on the last level.
		for (size_t i = 0; i < PTES_PER_LEVEL; ++i, ++Pte)
		{
			if (*Pte)
			{
				// The PTE exists, check if it was decommitted though.
				if (!MM_PTE_ISPRESENT(*Pte) && (*Pte & MM_DPTE_DECOMMITTED))
					continue;
				
				// This mapping level is busy.
				return false;
			}
		}
		
		return true;
	}
	
	bool FreeParent = true;
	
	// Walk the PML.
	for (size_t i = 0; i < PTES_PER_LEVEL; ++i, ++Pte)
	{
		if (!MM_PTE_ISPRESENT(*Pte))
			continue;
		
		PMMPTE SubPte = MiGetSubPteAddress(Pte);
		
		if (MmpIsPteListCompletelyEmpty(SubPte))
		{
			MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
			*Pte = 0;
			continue;
		}
		
		if (!MmpFreeUnusedMappingLevelsInCurrentMapPML(SubPte, MapLevel - 1))
		{
			FreeParent = false;
			continue;
		}
		
		// Returned true, so this is now ready to free.
		MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
		*Pte = 0;
	}
	
	return FreeParent;
}
