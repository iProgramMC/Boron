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
#include <arch/i386.h>

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
	ASSERT(Address < MI_PML1_LOCATION || (uint64_t)Address >= MI_PML1_LOC_END);
	
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
	// Allocate the PML2.
	int NewPageMappingPFN = MmAllocatePhysicalPage();
	if (NewPageMappingPFN == PFN_INVALID)
	{
		LogMsg("Error, can't create a new page mapping.  Can't allocate PML4");
		return 0;
	}
	
	uintptr_t NewPageMappingResult = MmPFNToPhysPage (NewPageMappingPFN);
	PMMPTE NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMappingResult), OldPageMappingAccess;
	//PMMPTE OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
	
	// copy the kernel's 256 entries, and zero out the first 256
	for (int i = 0; i < 256; i++)
	{
		NewPageMappingAccess[i] = 0;
	}
	
	// Lock the kernel space's lock to not get any surprises.
	MmLockKernelSpaceShared();
	
	for (int i = 256; i < 512; i++)
	{
		//NewPageMappingAccess[i] = OldPageMappingAccess[i];
		
		// We can't do it that easily because the new tree may be part of
		// a different 8MB window. This is slow, but for now I don't really
		// care!  (In the best case, both PMLs are in the same 8MB section
		// and no switching is performed)
		MMPTE Temp;
		
		OldPageMappingAccess = MmGetHHDMOffsetAddr (OldPageMapping);
		Temp = OldPageMappingAccess[i];
		
		NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMappingResult);
		NewPageMappingAccess[i] = Temp;
	}
	
	// For recursive paging
	NewPageMappingAccess[MI_RECURSIVE_PAGING_START] = (uintptr_t)NewPageMappingResult | MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_NOEXEC;
	
	MmUnlockKernelSpace();
	
	return (HPAGEMAP) NewPageMappingResult;
}

PMMPTE MiGetPTEPointer(HPAGEMAP Mapping, uintptr_t Address, bool AllocateMissingPMLs)
{
	const uintptr_t TenTwentyThreeMask = 0x3FF;
	
	uintptr_t indices[] = {
		0,
		(Address >> 12) & TenTwentyThreeMask,
		(Address >> 22) & TenTwentyThreeMask,
		0,
	};
	
	int    NumPfnsAllocated = 0;
	int    PfnsAllocated[3];
	MMPTE  PtesOriginals[3];
	PMMPTE PtesModified [3];
	
	MMPTE SupervisorBit;
	if (Address >= MM_KERNEL_SPACE_BASE)
		SupervisorBit = 0;
	else
		SupervisorBit = MM_PTE_USERACCESS;
	
	HPAGEMAP CurrentLevel = Mapping;
	PMMPTE EntryPointer = NULL;
	
	for (int pml = 2; pml >= 1; pml--)
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
				
				return NULL;
			}
			
			PtesModified [NumPfnsAllocated] = EntryPointer;
			PtesOriginals[NumPfnsAllocated] = Entry;
			PfnsAllocated[NumPfnsAllocated] = pfn;
			NumPfnsAllocated++;
			
			memset(MmGetHHDMOffsetAddr(MmPFNToPhysPage(pfn)), 0, PAGE_SIZE);
			
			*EntryPointer = Entry = MM_PTE_PRESENT | MM_PTE_READWRITE | SupervisorBit | MmPFNToPhysPage(pfn);
		}
		
		if (pml > 1 && (Entry & MM_PTE_PAGESIZE))
		{
			// Higher page size, we can't allocate here.  Probably HHDM or something - the kernel itself doesn't use this
			DbgPrint("MiGetPTEPointer: Address %p contains a higher page size, we don't support that for now", Address);
			return NULL;
		}
		
		CurrentLevel = Entry & MM_PTE_ADDRESSMASK;
	}
	
	return EntryPointer;
}

static void MmpFreeVacantPMLsSub(HPAGEMAP Mapping, uintptr_t Address)
{
	// Lot of code was copied from MiGetPTEPointer.
	const uintptr_t TenTwentyThreeMask = 0x3FF;
	
	uintptr_t indices[] = {
		0,
		(Address >> 12) & TenTwentyThreeMask,
		(Address >> 22) & TenTwentyThreeMask,
		0,
	};
	
	HPAGEMAP CurrentLevel = Mapping;
	PMMPTE EntryPointer = NULL, ParentEntryPointer = NULL;
	
	for (int pml = 2; pml >= 1; pml--)
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
			for (int i = 0; i < 1024; i++)
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

static void MmpFreeVacantPMLs(HPAGEMAP Mapping, uintptr_t Address)
{
	// Do this 2 times to ensure all levels are freed.  Could be done better
	for (int i = 0; i < 2; i++)
		MmpFreeVacantPMLsSub(Mapping, Address);
}

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

bool MiMapAnonPage(HPAGEMAP Mapping, uintptr_t Address, uintptr_t Permissions, bool NonPaged)
{
	PMMPTE Pte = MiGetPTEPointer(Mapping, Address, true);
	
	return MmpMapSingleAnonPageAtPte(Pte, Permissions, NonPaged);
}

bool MiMapPhysicalPage(HPAGEMAP Mapping, uintptr_t PhysicalPage, uintptr_t Address, uintptr_t Permissions)
{
	PMMPTE Pte = MiGetPTEPointer(Mapping, Address, true);
	
	if (!Pte)
		return false;
	
	*Pte = (PhysicalPage & MM_PTE_ADDRESSMASK) | Permissions | MM_PTE_PRESENT;
	
	return true;
}

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
		MM_PTE_GLOBAL     |
		MM_PTE_ISFROMPMM  |
		MM_PTE_NOEXEC     |
		MmPFNToPhysPage(Pfn);
}

uintptr_t MiGetTopOfPoolManagedArea()
{
	return MI_GLOBAL_AREA_START << 22;
}

uintptr_t MiGetTopOfSecondPoolManagedArea()
{
	return MI_GLOBAL_AREA_START_2ND << 22;
}

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
