/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/amd64/pt.c
	
Abstract:
	This module implements page table management for
	the AMD64 platform.
	
Author:
	iProgramInCpp - 8 September 2023
***/

#include <main.h>
#include <string.h>
#include <mm.h>
#include <arch/amd64.h>

// Creates a page mapping.
PageMapping MmCreatePageMapping(PageMapping OldPageMapping)
{
	// Allocate the PML4.
	int NewPageMappingPFN = MmAllocatePhysicalPage();
	if (NewPageMappingPFN == PFN_INVALID)
	{
		LogMsg("Error, can't create a new page mapping.  Can't allocate PML4");
		return 0;
	}
	
	uintptr_t NewPageMappingResult = MmPFNToPhysPage (NewPageMappingPFN);
	PageTableEntry* NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMappingResult);
	PageTableEntry* OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
	// copy the kernel's 256 entries, and zero out the first 256
	for (int i = 0; i < 256; i++)
	{
		NewPageMappingAccess[i] = 0;
	}
	
	for (int i = 256; i < 512; i++)
	{
		NewPageMappingAccess[i] = OldPageMappingAccess[i];
	}
	
	return (PageMapping) NewPageMappingResult;
}

bool MmpCloneUserHalfLevel(int Level, PageTableEntry* New, PageTableEntry* Old, int Index)
{
	New[Index] = 0;
	
	int PageForThisLevelPFN = MmAllocatePhysicalPage();
	if (PageForThisLevelPFN == PFN_INVALID)
		return false;
	
	if (~Old[Index] & MM_PTE_PRESENT)
	{
		// TODO handle a non present page... Currently just exit.
		New[Index] = 0;
		return true;
	}
	
	if (Level == 1)
	{
		// TODO: Copy on write
		// return true;
	}
	
	uintptr_t PageForThisLevel = MmPFNToPhysPage (PageForThisLevelPFN);
	
	// write it to the new with the same flags as old, except that we set the PMM flag
	New[Index] = PageForThisLevel | (Old[Index] & 0xFFF) | MM_PTE_ISFROMPMM;
	
	PageTableEntry* PageAccess = MmGetHHDMOffsetAddr (PageForThisLevel);
	
	if (Level == 1)
	{
		// Copy the contents of the page.
		PageTableEntry* OldPageAccess = MmGetHHDMOffsetAddr (Old[Index] & MM_PTE_ADDRESSMASK);
		
		memcpy (PageAccess, OldPageAccess, PAGE_SIZE);
		
		return true;
	}
	
	// Recursively copy the next level.
	for (int i = 0; i < 512; i++)
	{
		PageTableEntry* OldPageAccess = MmGetHHDMOffsetAddr (Old[Index] & MM_PTE_ADDRESSMASK);
		
		MmpCloneUserHalfLevel (Level - 1, PageAccess, OldPageAccess, i);
	}
	
	return true;
}

// Clones a user page mapping
PageMapping MmClonePageMapping(PageMapping OldPageMapping)
{
	PageMapping NewPageMapping = MmCreatePageMapping(OldPageMapping);
	
	// If the new page mapping is zero, we can't proceed!
	if (NewPageMapping == 0)
		return 0;
	
	PageTableEntry* NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMapping);
	PageTableEntry* OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
	// Clone the user half now.
	for (int i = 0; i < 256; i++)
	{
		if (!MmpCloneUserHalfLevel(4, NewPageMappingAccess, OldPageMappingAccess, i))
		{
			//LogMsg("Error, can't fully clone a page mapping. i = %d", i);
			
			// TODO: rollback all clones so far
			
			MmFreePhysicalPageHHDM(NewPageMappingAccess);
			return 0;
		}
	}
	
	return NewPageMapping;
}

// Free an entry on the PML<level> of a page mapping
void MmpFreePageMappingLevel(uint64_t PageMappingLevel, int Index, int Level)
{
    // Convert the physical address to virtual address for inspection
    PageTableEntry* PageMappingAccess = MmGetHHDMOffsetAddr(PageMappingLevel);
	
	if (~PageMappingAccess[Index] & MM_PTE_PRESENT)
	{
		// TODO: Handle non present pages.
		return;
	}
	
	// Get the current page from the entry we're processing.
	uint64_t PageAddress = PageMappingAccess[Index] & MM_PTE_ADDRESSMASK;

    // If this is not the final level, then traverse deeper.
    if (Level > 1 && (~PageMappingAccess[Index] & MM_PTE_PAGESIZE)) 
    {
        // Recursively traverse the child entries
        for (int i = 0; i < 512; i++) 
        {
            MmpFreePageMappingLevel(PageAddress, i, Level - 1);
        }
    }

    // Now, check MM_PTE_ISFROMPMM flag and free the page (common for all levels)
    if (PageMappingAccess[Index] & MM_PTE_ISFROMPMM) 
    {
		MmFreePhysicalPage(MmPhysPageToPFN(PageAddress));
    }
}

void MmFreePageMapping(PageMapping OldPageMapping)
{
	for (int i = 0; i < 512; i++)
	{
		MmpFreePageMappingLevel(OldPageMapping, i, 3);
	}
	
	// free the old page mapping itself
	MmFreePhysicalPage(MmPhysPageToPFN(OldPageMapping));
}

PageTableEntry* MmGetPTEPointer(PageMapping Mapping, uintptr_t Address, bool AllocateMissingPMLs)
{
	const uintptr_t FiveElevenMask = 0x1FF;
	
	uintptr_t indices[] = {
		0,
		(Address >> 12) & FiveElevenMask,
		(Address >> 21) & FiveElevenMask,
		(Address >> 30) & FiveElevenMask,
		(Address >> 39) & FiveElevenMask,
		0,
	};
	
	int          NumPfnsAllocated = 0;
	int             PfnsAllocated[5];
	PageTableEntry  PtesOriginals[5];
	PageTableEntry* PtesModified [5];
	
	PageMapping CurrentLevel = Mapping;
	PageTableEntry* EntryPointer = NULL;
	
	for (int pml = 4; pml >= 1; pml--)
	{
		PageTableEntry* Entries = MmGetHHDMOffsetAddr(CurrentLevel);
		
		EntryPointer = &Entries[indices[pml]];
		
		PageTableEntry Entry = *EntryPointer;
		
		if (pml > 1 && (~Entry & MM_PTE_PRESENT))
		{
			// not present!! Do we allocate it?
			if (!AllocateMissingPMLs)
				return NULL;
			
			int pfn = MmAllocatePhysicalPage();
			if (pfn == PFN_INVALID)
			{
				//SLogMsg("MmGetPTEPointer: Ran out of memory trying to allocate PTEs along the PML path");
				
				// rollback
				for (int i = 0; i < NumPfnsAllocated; i++)
				{
					*(PtesModified[i]) = PtesOriginals[i];
					MmFreePhysicalPage(PfnsAllocated[i]);
				}
				
				return false;
			}
			
			PtesModified [NumPfnsAllocated] = EntryPointer;
			PtesOriginals[NumPfnsAllocated] = Entry;
			PfnsAllocated[NumPfnsAllocated] = pfn;
			NumPfnsAllocated++;
			
			memset(MmGetHHDMOffsetAddr(MmPFNToPhysPage(pfn)), 0, PAGE_SIZE);
			
			*EntryPointer = Entry = MM_PTE_PRESENT | MM_PTE_READWRITE | MmPFNToPhysPage(pfn);
		}
		
		if (pml > 1 && (Entry & MM_PTE_PAGESIZE))
		{
			// Higher page size, we can't allocate here.  Probably HHDM or something - the kernel itself doesn't use this
			//SLogMsg("MmGetPTEPointer: Address %p contains a higher page size, we don't support that for now", Address);
			return NULL;
		}
		
		//SLogMsg("DUMPING PML%d (got here from PML%d[%d]) :", pml, pml + 1, indices[pml + 1]);
		//for (int i = 0; i < 512; i++)
		//	SLogMsg("[%d] = %p", i, Entries[i]);
		
		CurrentLevel = Entry & MM_PTE_ADDRESSMASK;
	}
	
	return EntryPointer;
}

static void MmpFreeVacantPMLsSub(PageMapping Mapping, uintptr_t Address)
{
	// Lot of code was copied from MmGetPTEPointer.
	const uintptr_t FiveElevenMask = 0x1FF;
	
	uintptr_t indices[] = {
		0,
		(Address >> 12) & FiveElevenMask,
		(Address >> 21) & FiveElevenMask,
		(Address >> 30) & FiveElevenMask,
		(Address >> 39) & FiveElevenMask,
		0,
	};
	
	PageMapping CurrentLevel = Mapping;
	PageTableEntry *EntryPointer = NULL, *ParentEntryPointer = NULL;
	
	for (int pml = 4; pml >= 1; pml--)
	{
		PageTableEntry* Entries = MmGetHHDMOffsetAddr(CurrentLevel);
		
		EntryPointer = &Entries[indices[pml]];
		
		PageTableEntry Entry = *EntryPointer;
		
		if (pml > 1 && (~Entry & MM_PTE_PRESENT))
		{
			// if we don't have a parent, return
			if (!ParentEntryPointer)
				return;
			
			// check if this entire page is vacant
			for (int i = 0; i < 512; i++)
			{
				if (Entries[i] != 0)
					return; // isn't vacant
			}
			
			// is vacant, so free it
			*ParentEntryPointer = 0;
			
			int pfn = MmPhysPageToPFN(CurrentLevel);
			MmFreePhysicalPage(pfn);
		}
		
		if (pml > 1 && (Entry & MM_PTE_PAGESIZE))
		{
			// Higher page size, we can't do that here.  Probably HHDM or something - the kernel itself doesn't use this
			return;
		}
		
		CurrentLevel = Entry & MM_PTE_ADDRESSMASK;
		ParentEntryPointer = EntryPointer;
	}
}

static void MmpFreeVacantPMLs(PageMapping Mapping, uintptr_t Address)
{
	// Do this 4 times to ensure all levels are freed.  Could be done better
	for (int i = 0; i < 4; i++)
		MmpFreeVacantPMLsSub(Mapping, Address);
}

bool MmMapAnonPage(PageMapping Mapping, uintptr_t Address, uintptr_t Permissions)
{
	PageTableEntry* pPTE = MmGetPTEPointer(Mapping, Address, true);
	
	int pfn = MmAllocatePhysicalPage();
	if (pfn == PFN_INVALID)
	{
		//SLogMsg("MmMapAnonPage(%p, %p) failed because we couldn't allocate physical memory", Mapping, Address);
		return false;
	}
	
	if (!pPTE)
	{
		//SLogMsg("MmMapAnonPage(%p, %p) failed because PTE couldn't be retrieved", Mapping, Address);
		return false;
	}
	
	*pPTE = MM_PTE_PRESENT | Permissions | MmPFNToPhysPage(pfn);
	
	return true;
}

void MmUnmapPages(PageMapping Mapping, uintptr_t Address, size_t LengthPages)
{
	// Step 1. Unset the PRESENT bit on all pages in the range.
	for (size_t i = 0; i < LengthPages; i++)
	{
		PageTableEntry* pPTE = MmGetPTEPointer(Mapping, Address + i * PAGE_SIZE, false);
		
		if (!pPTE)
			continue;
		
		if (*pPTE & MM_PTE_PRESENT)
		{
			*pPTE &= ~MM_PTE_PRESENT;
			*pPTE |= MM_DPTE_WASPRESENT;
		}
		else
		{
			*pPTE &= ~MM_DPTE_WASPRESENT;
		}
	}
	
	// Step 2. Issue a single TLB shootdown command to all CPUs to flush the TLB.
	// TODO: This could be optimized, but eh, it's fine for now..
	MmIssueTLBShootDown(Address, LengthPages);
	
	// Step 3. If needed, free the PMM pages related to this page mapping.
	for (size_t i = 0; i < LengthPages; i++)
	{
		PageTableEntry* pPTE = MmGetPTEPointer(Mapping, Address + i * PAGE_SIZE, false);
		
		if (!pPTE)
			continue;
		
		if (*pPTE & MM_DPTE_WASPRESENT)
		{
			uintptr_t PhysPage = *pPTE & MM_PTE_ADDRESSMASK;
			
			MmFreePhysicalPage(MmPhysPageToPFN(PhysPage));
			
			*pPTE = 0;
		}
	}
	
	return;
	
	// Step 4. Free higher PMLs if they're fully vacant
	for (size_t i = 0; i < LengthPages; i++)
	{
		MmpFreeVacantPMLs(Mapping, Address + i * PAGE_SIZE);
	}
}
