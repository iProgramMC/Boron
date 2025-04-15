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
#include <ke.h>
#include <arch/amd64.h>

#define MI_PTE_LOC(Address) (MI_PML1_LOCATION + ((Address & MI_PML_ADDRMASK) >> 12) * sizeof(MMPTE))

PMMPTE MmGetPteLocation(uintptr_t Address)
{
	PMMPTE PtePtr = (PMMPTE)MI_PTE_LOC(Address);
	
	// HACK: Instead of just invalidating everything in the function
	// MiFreeUnusedMappingLevelsInCurrentMap like I am supposed to,
	// I will invalidate the TLB here.
	//
	// I know this is bad, but come on, when are we *ever* going to
	// *not* go through this function?
	KeInvalidatePage(PtePtr);
	
	return PtePtr;
}

bool MmCheckPteLocation(uintptr_t Address, bool GenerateMissingLevels)
{
	ASSERT(Address < MI_PML1_LOCATION || Address >= MI_PML1_LOC_END);
	
	// Check PML4, that's always accessible.
	PMMPTE Pte;
	
	Pte = MmGetPteLocation(MI_PTE_LOC(MI_PTE_LOC(MI_PTE_LOC(Address))));
	if (~(*Pte) & MM_PTE_PRESENT)
		goto Missing;
	
	// PML4 exists, check PML3
	Pte = MmGetPteLocation(MI_PTE_LOC(MI_PTE_LOC(Address)));
	if (~(*Pte) & MM_PTE_PRESENT)
		goto Missing;
	
	// PML3 exists, check PML2
	Pte = MmGetPteLocation(MI_PTE_LOC(Address));
	if (~(*Pte) & MM_PTE_PRESENT)
		goto Missing;
	
	// PML2 exists, check PML1
	Pte = MmGetPteLocation(Address);
	if (~(*Pte) & MM_PTE_PRESENT)
	{
	Missing:
		if (GenerateMissingLevels)
		{
			PMMPTE Pte = MiGetPTEPointer(MiGetCurrentPageMap(), Address, GenerateMissingLevels);
			return Pte != NULL;
		}
		
		return false;
	}
	
	return true;
}

PMMPTE MmGetPteLocationCheck(uintptr_t Address, bool GenerateMissingLevels)
{
	if (!MmCheckPteLocation(Address, GenerateMissingLevels))
		return NULL;
	
	return MmGetPteLocation(Address);
}

// Creates a page mapping.
HPAGEMAP MiCreatePageMapping(HPAGEMAP OldPageMapping)
{
	// Allocate the PML4.
	int NewPageMappingPFN = MmAllocatePhysicalPage();
	if (NewPageMappingPFN == PFN_INVALID)
	{
		LogMsg("Error, can't create a new page mapping.  Can't allocate PML4");
		return 0;
	}
	
	uintptr_t NewPageMappingResult = MmPFNToPhysPage (NewPageMappingPFN);
	PMMPTE NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMappingResult);
	PMMPTE OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
	// copy the kernel's 256 entries, and zero out the first 256
	for (int i = 0; i < 256; i++)
	{
		NewPageMappingAccess[i] = 0;
	}
	
	// Lock the kernel space's lock to not get any surprises.
	MmLockKernelSpaceShared();
	
	for (int i = 256; i < 512; i++)
	{
		NewPageMappingAccess[i] = OldPageMappingAccess[i];
	}
	
	// For recursive paging
	NewPageMappingAccess[MI_RECURSIVE_PAGING_START] = (uintptr_t)NewPageMappingResult | MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_SUPERVISOR | MM_PTE_NOEXEC;
	
	MmUnlockKernelSpace();
	
	return (HPAGEMAP) NewPageMappingResult;
}

// TODO: this will most likely be rewritten
bool MmpCloneUserHalfLevel(int Level, PMMPTE New, PMMPTE Old, int Index)
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
	
	PMMPTE PageAccess = MmGetHHDMOffsetAddr (PageForThisLevel);
	
	if (Level == 1)
	{
		// Copy the contents of the page.
		PMMPTE OldPageAccess = MmGetHHDMOffsetAddr (Old[Index] & MM_PTE_ADDRESSMASK);
		
		memcpy (PageAccess, OldPageAccess, PAGE_SIZE);
		
		return true;
	}
	
	// Recursively copy the next level.
	for (int i = 0; i < 512; i++)
	{
		PMMPTE OldPageAccess = MmGetHHDMOffsetAddr (Old[Index] & MM_PTE_ADDRESSMASK);
		
		MmpCloneUserHalfLevel (Level - 1, PageAccess, OldPageAccess, i);
	}
	
	return true;
}

// TODO: this will most likely be rewritten
// Clones a user page mapping
HPAGEMAP MmClonePageMapping(HPAGEMAP OldPageMapping)
{
	HPAGEMAP NewPageMapping = MiCreatePageMapping(OldPageMapping);
	
	// If the new page mapping is zero, we can't proceed!
	if (NewPageMapping == 0)
		return 0;
	
	PMMPTE NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMapping);
	PMMPTE OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
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

/***
	Function description:
		Gets the PTE (Page Table Entry) pointer for the specified address.
		
		If AllocateMissingPMLs is set, also creates the page mapping levels
		along the way, otherwise, if the levels aren't already there, this
		return NULL.
		
		This function also returns NULL if allocation of one of the PMLs fails.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		Address  - The starting address of the range to map
		
		AllocateMissingPMLs - See function description.
	
	Return value:
		The address of the PTE, or NULL. See function description on why
		it would return NULL.
***/
PMMPTE MiGetPTEPointer(HPAGEMAP Mapping, uintptr_t Address, bool AllocateMissingPMLs)
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
	
	int    NumPfnsAllocated = 0;
	int    PfnsAllocated[5];
	MMPTE  PtesOriginals[5];
	PMMPTE PtesModified [5];
	
	HPAGEMAP CurrentLevel = Mapping;
	PMMPTE EntryPointer = NULL;
	
	for (int pml = 4; pml >= 1; pml--)
	{
		PMMPTE Entries = MmGetHHDMOffsetAddr(CurrentLevel);
		
		EntryPointer = &Entries[indices[pml]];
		
		MMPTE Entry = *EntryPointer;
		
		if (pml > 1 && (~Entry & MM_PTE_PRESENT))
		{
			// not present!! Do we allocate it?
			if (!AllocateMissingPMLs)
				return NULL;
			
			int pfn = MmAllocatePhysicalPage();
			if (pfn == PFN_INVALID)
			{
				DbgPrint("MiGetPTEPointer: Ran out of memory trying to allocate PTEs along the PML path");
				
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
			//DbgPrint("MiGetPTEPointer: Address %p contains a higher page size, we don't support that for now", Address);
			return NULL;
		}
		
		//DbgPrint("DUMPING PML%d (got here from PML%d[%d]) :", pml, pml + 1, indices[pml + 1]);
		//for (int i = 0; i < 512; i++)
		//	DbgPrint("[%d] = %p", i, Entries[i]);
		
		CurrentLevel = Entry & MM_PTE_ADDRESSMASK;
	}
	
	return EntryPointer;
}

/***
	Function description:
		Frees the lowest PML at the specified address if it is vacant.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		Address  - The starting address of the range to map
	
	Return value:
		None.
***/
static void MmpFreeVacantPMLsSub(HPAGEMAP Mapping, uintptr_t Address)
{
	// Lot of code was copied from MiGetPTEPointer.
	const uintptr_t FiveElevenMask = 0x1FF;
	
	uintptr_t indices[] = {
		0,
		(Address >> 12) & FiveElevenMask,
		(Address >> 21) & FiveElevenMask,
		(Address >> 30) & FiveElevenMask,
		(Address >> 39) & FiveElevenMask,
		0,
	};
	
	HPAGEMAP CurrentLevel = Mapping;
	PMMPTE EntryPointer = NULL, ParentEntryPointer = NULL;
	
	for (int pml = 4; pml >= 1; pml--)
	{
		PMMPTE Entries = MmGetHHDMOffsetAddr(CurrentLevel);
		
		EntryPointer = &Entries[indices[pml]];
		
		MMPTE Entry = *EntryPointer;
		
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

/***
	Function description:
		Frees vacant PMLs (Page Mapping Levels) at a specified address.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		Address  - The starting address of the range to map
	
	Return value:
		None.
***/
static void MmpFreeVacantPMLs(HPAGEMAP Mapping, uintptr_t Address)
{
	// Do this 4 times to ensure all levels are freed.  Could be done better
	for (int i = 0; i < 4; i++)
		MmpFreeVacantPMLsSub(Mapping, Address);
}

/***
	Function description:
		Maps a single anonymous-backed page at a certain PTE pointer.
		MiMapAnonPage and MiMapAnonPages use this function behind the scenes.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		Address  - The starting address of the range to map
		
		SizePages - The size (in pages) of the mapped range
		
		Permissions - The permission bits of the mapped range
		
		NonPaged - Whether to allow page faults on the mapped range
	
	Return value:
		Whether the mapping update was successful.
***/
static bool MmpMapSingleAnonPageAtPte(PMMPTE Pte, uintptr_t Permissions, bool NonPaged)
{
	if (!Pte)
		return false;
	
    if (MM_DBG_NO_DEMAND_PAGING || NonPaged)
	{
		int pfn = MmAllocatePhysicalPage();
		if (pfn == PFN_INVALID)
		{
			//DbgPrint("MiMapAnonPage(%p, %p) failed because we couldn't allocate physical memory", Mapping, Address);
			return false;
		}
		
		if (!Pte)
		{
			//DbgPrint("MiMapAnonPage(%p, %p) failed because PTE couldn't be retrieved", Mapping, Address);
			return false;
		}
		
		*Pte = MM_PTE_PRESENT | MM_PTE_ISFROMPMM | Permissions | MmPFNToPhysPage(pfn);
		return true;
	}
	
	*Pte = MM_DPTE_COMMITTED | Permissions;
	
	return true;
}

/***
	Function description:
		Maps a single anonymous-backed page at an address.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		Address  - The starting address of the range to map
		
		SizePages - The size (in pages) of the mapped range
		
		Permissions - The permission bits of the mapped range
		
		NonPaged - Whether to allow page faults on the mapped range
	
	Return value:
		Whether the mapping update was successful.
***/
bool MiMapAnonPage(HPAGEMAP Mapping, uintptr_t Address, uintptr_t Permissions, bool NonPaged)
{
	PMMPTE Pte = MiGetPTEPointer(Mapping, Address, true);
	
	return MmpMapSingleAnonPageAtPte(Pte, Permissions, NonPaged);
}

/***
	Function description:
		Maps a single page of physical memory at a known physical
		address, to a virtual address, using the specified page
		mapping.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		PhysicalPage - The physical address of the page to map
		
		Address  - The starting address of the range to map
		
		Permissions - The permission bits of the mapped range
		
		NonPaged - Whether to allow page faults on the mapped range
	
	Return value:
		Whether the mapping update was successful.
***/
bool MiMapPhysicalPage(HPAGEMAP Mapping, uintptr_t PhysicalPage, uintptr_t Address, uintptr_t Permissions)
{
	PMMPTE Pte = MiGetPTEPointer(Mapping, Address, true);
	
	if (!Pte)
		return false;
	
	*Pte = (PhysicalPage & MM_PTE_ADDRESSMASK) | Permissions | MM_PTE_PRESENT;
	
	return true;
}

/***
	Function description:
		Unmap a range of memory from the specified page mapping.
		Issues TLB shootdowns as necessary (though right now it may be
		a little bit overzealous with them)
	
	Parameters:
		Mapping - The page map to be modified.
	
	Return value:
		None.
***/
void MiUnmapPages(HPAGEMAP Mapping, uintptr_t Address, size_t LengthPages)
{
	// Step 1. Unset the PRESENT bit on all pages in the range.
	for (size_t i = 0; i < LengthPages; i++)
	{
		PMMPTE pPTE = MiGetPTEPointer(Mapping, Address + i * PAGE_SIZE, false);
		
		if (!pPTE)
			continue;
		
		*pPTE &= ~MM_DPTE_COMMITTED;
		
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
		PMMPTE pPTE = MiGetPTEPointer(Mapping, Address + i * PAGE_SIZE, false);
		
		if (!pPTE)
			continue;
		
		uintptr_t Flags = MM_DPTE_WASPRESENT | MM_PTE_ISFROMPMM;
		
		if ((*pPTE & Flags) == Flags)
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

/***
	Function description:
		Prepares the pool manager. Since this is hardware specific,
		this was split away from the pool manager.
	
	Parameters:
		The page map to be modified.  Ideally, this function affects
		ALL page maps.
	
	Return value:
		None.
	
	Notes:
		This function is run during uniprocessor system initialization.
***/
void MiPrepareGlobalAreaForPool(HPAGEMAP PageMap)
{
	PMMPTE Ptes = MmGetHHDMOffsetAddr(PageMap);
	
	int Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
	{
		KeCrashBeforeSMPInit("MiPrepareGlobalAreaForPool: Can't allocate global pml4");
	}
	
	Ptes[MI_GLOBAL_AREA_START] = 
		MM_PTE_PRESENT    |
		MM_PTE_READWRITE  |
		MM_PTE_SUPERVISOR |
		MM_PTE_GLOBAL     |
		MM_PTE_ISFROMPMM  |
		MM_PTE_NOEXEC     |
		MmPFNToPhysPage(Pfn);
}

/***
	Function description:
		Gets the top of the pool managed area. This was split away
		from the pool manager code because it's hardware specific
		what range of memory the pool manager has access to.
	
	Parameters:
		None.
	
	Return value:
		The starting address of the memory range managed by the
		pool manager.
***/
uintptr_t MiGetTopOfPoolManagedArea()
{
	// Sign extension | the start of the area managed by the PML4 index we specified.
	// If we didn't have a sign extension, we'd have a non-canonical address, which we
	// can never actually map to.
	return 0xFFFF000000000000 | (MI_GLOBAL_AREA_START << 39);
}

/***
	Function description:
		Marks a series of addresses as backed with anonymous memory.
		
		If NonPaged is set, memory will be instantly reserved for this range, i.e.
		one can access the memory from any IPL (since no page faults are taken).
		
		If NonPaged is clear, memory is not instantly reserved, and accessing each
		corresponding page will incur one minor page fault.
	
	XXX OUTDATED: See MmCommitVirtualMemory for a newer implementation.
	
	Parameters:
		Mapping  - The handle to the page table to be modified
		
		Address  - The starting address of the range to map
		
		SizePages - The size (in pages) of the mapped range
		
		Permissions - The permission bits of the mapped range.
		
		NonPaged - See the function description.
	
	Return value:
		Whether the allocation was successful.
***/
bool MiMapAnonPages(HPAGEMAP Mapping, uintptr_t Address, size_t SizePages, uintptr_t Permissions, bool NonPaged)
{
	// As an optimization, we'll wait until the PML1 index rolls over to zero before reloading the PTE pointer.
	uint64_t CurrentPml1 = PML1_IDX(Address);
	size_t DonePages = 0;
	
	PMMPTE PtePtr = MiGetPTEPointer(Mapping, Address, true);
	
	for (size_t i = 0; i < SizePages; i++)
	{
		// If one of these fails, then we should roll back.
		if (!MmpMapSingleAnonPageAtPte(PtePtr, Permissions, NonPaged))
			goto ROLLBACK;
		
		// Increase the address size, get the next PTE pointer, update the current PML1, and
		// increment the number of mapped pages (since this one was successfully mapped).
		Address += PAGE_SIZE;
		PtePtr++;
		CurrentPml1++;
		DonePages++;
		
		if (CurrentPml1 % (PAGE_SIZE / sizeof(MMPTE)) == 0)
		{
			// We have rolled over.
			PtePtr = MiGetPTEPointer(Mapping, Address, true);
		}
	}
	
	// All allocations have succeeded! Let the caller know and don't undo our work. :)
	return true;
	
ROLLBACK:
	// Unmap all the pages that we have mapped.
	MiUnmapPages(Mapping, Address, DonePages);
	return false;
}

MMPTE MmGetPteBitsFromProtection(int Protection)
{
	MMPTE Pte = 0;
	
	if (Protection & PAGE_WRITE)
		Pte |= MM_PTE_READWRITE;
	
	if (~Protection & PAGE_EXECUTE)
		Pte |= MM_PTE_NOEXEC;
	
	return Pte;
}
