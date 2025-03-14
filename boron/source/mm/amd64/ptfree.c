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

#define PTES_PER_LEVEL (PAGE_SIZE / sizeof(MMPTE)) // 512


// Gets an address down the tree.
// For example, if you're in the PML4, this will give you
// the relevant PML2 address.
PMMPTE MiGetSubPteAddress(PMMPTE PteAddress)
{
	MMADDRESS_CONVERT Address;
	Address.Long = (uintptr_t) PteAddress;
	
	Address.Level4 = Address.Level3;
	Address.Level3 = Address.Level2;
	Address.Level2 = Address.Level1;
	Address.Level1 = Address.PageOffset / sizeof(PMMPTE);
	Address.PageOffset = 0;
	Address.SignExtension = 0xFFFF;
	
	return (PMMPTE) Address.Long;
}

bool MmpFreeUnusedMappingLevelsInCurrentMapPML(PMMPTE Pte, int RecursionCount);

// NOTE: StartVa and SizePages are only roughly followed.
void MiFreeUnusedMappingLevelsInCurrentMap(uintptr_t StartVa, size_t SizePages)
{
	// Walk the PML4.
	uintptr_t Index = (StartVa >> 39) & 0x1FF;
	PMMPTE Pte = (PMMPTE) MI_PML3_LOCATION + Index;
	
	ASSERT(Index < 256);
	
	for (size_t i = 0; i < SizePages; i += PTES_PER_LEVEL * PTES_PER_LEVEL * PTES_PER_LEVEL, ++Pte)
	{
		if (~*Pte & MM_PTE_PRESENT)
			// The PTE is not present. Skip.
			continue;
		
		// The PTE is present.
		// Check to see if it is all zeroes.
		PMMPTE SubPte = MiGetSubPteAddress(Pte);
		bool Ok = true;
		
		for (size_t j = 0; Ok && j < PTES_PER_LEVEL; j++)
		{
			if (SubPte[j] != 0)
				Ok = false;
		}
		
		if (Ok)
		{
			// This is free already.
			MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
			*Pte = 0;
			continue;
		}
		
		// Look at the level below.
		if (!MmpFreeUnusedMappingLevelsInCurrentMapPML(SubPte, 3))
			continue;
		
		// Returned true, so this is now ready to free.
		MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
		*Pte = 0;
	}
}

bool MmpFreeUnusedMappingLevelsInCurrentMapPML(PMMPTE Pte, int RecursionCount)
{
	if (RecursionCount <= 1)
	{
		// We're on the last level.
		for (size_t i = 0; i < PTES_PER_LEVEL; ++i, ++Pte)
		{
			if (*Pte)
			{
				// The PTE exists, check if it was decommitted though.
				if ((~*Pte & MM_PTE_PRESENT) && (*Pte & MM_DPTE_DECOMMITTED))
					continue;
				
				// The PTE exists and isn't decommitted, therefore this mapping level is busy.
				return false;
			}
		}
		
		return true;
	}
	
	bool FreeParent = true;
	
	// Walk the PML.
	for (size_t i = 0; i < PTES_PER_LEVEL; ++i, ++Pte)
	{
		if (~*Pte & MM_PTE_PRESENT)
		{
			// The PTE is not present. Skip.
			continue;
		}
		
		// The PTE is present.
		// Check to see if it is all zeroes.
		PMMPTE SubPte = MiGetSubPteAddress(Pte);
		bool Ok = true;
		
		for (size_t j = 0; Ok && j < PTES_PER_LEVEL; j++)
		{
			if (SubPte[j] != 0)
				Ok = false;
		}
		
		if (Ok)
		{
			// This is free already.
			MmFreePhysicalPage((*Pte & MI_PML_ADDRMASK) >> 12);
			*Pte = 0;
			continue;
		}
		
		// Look at the level below.
		if (!MmpFreeUnusedMappingLevelsInCurrentMapPML(SubPte, RecursionCount - 1))
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
