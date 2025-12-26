/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/i386/pt.c
	
Abstract:
	This module implements page table management for
	the i386 platform.
	
Author:
	iProgramInCpp - 15 October 2025
***/

#include <main.h>
#include <string.h>
#include <ke.h>
#include <arch/i386.h>
#include "../mi.h"

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
	PMMPTE Pte;
	MMPTE SupervisorBit;
	
	ASSERT(Address < MI_PML1_LOCATION || (uint64_t)Address >= MI_PML1_LOC_END);
	
	if (Address >= MM_KERNEL_SPACE_BASE)
		SupervisorBit = 0;
	else
		SupervisorBit = MM_PTE_USERACCESS;
	
	// Check the presence of the PT
	Pte = MmGetPteLocation(MI_PTE_LOC(Address));
	if (~(*Pte) & MM_PTE_PRESENT)
	{
		if (!GenerateMissingLevels)
			return false;
		
		MMPFN PtAllocated = MmAllocatePhysicalPage();
		if (PtAllocated == PFN_INVALID)
			return false;
		
		*Pte = MmPFNToPhysPage(PtAllocated) | MM_PTE_PRESENT | MM_PTE_READWRITE | SupervisorBit;
	}
	
	// Page table exists.
	return true;
}

PMMPTE MmGetPteLocationCheck(uintptr_t Address, bool GenerateMissingLevels)
{
	if (!MmCheckPteLocation(Address, GenerateMissingLevels))
		return NULL;
	
	return MmGetPteLocation(Address);
}

// Creates a page mapping.
HPAGEMAP MiCreatePageMapping()
{
	// Allocate the PML2.
	MMPFN NewPageMappingPFN = MmAllocatePhysicalPage();
	if (NewPageMappingPFN == PFN_INVALID)
	{
		LogMsg("Error, can't create a new page mapping.  Can't allocate PML4");
		return 0;
	}
	
	uintptr_t NewPageMappingResult = MmPFNToPhysPage (NewPageMappingPFN);
	
	// Lock the kernel space's lock to not get any surprises.
	MmLockKernelSpaceShared();
	MmBeginUsingHHDM();
	
	PMMPTE NewPageMappingAccess = MmGetHHDMOffsetAddr (NewPageMappingResult);
	PMMPTE OldPageDirectory = (PMMPTE) MI_PML2_LOCATION;
	
	// zero out the first 512
	for (int i = 0; i < 512; i++)
		NewPageMappingAccess[i] = 0;
	
	// then copy out the kernel's latter 512 entries
	for (int i = 512; i < 1024; i++)
		NewPageMappingAccess[i] = OldPageDirectory[i];
	
	// and replace that last entry with the pointer to this one.
	NewPageMappingAccess[MI_RECURSIVE_PAGING_START] = (uintptr_t)NewPageMappingResult | MM_PTE_PRESENT | MM_PTE_READWRITE | MM_PTE_NOEXEC;
	
	MmEndUsingHHDM();
	MmUnlockKernelSpace();
	return (HPAGEMAP) NewPageMappingResult;
}

// Frees a page mapping.
void MiFreePageMapping(HPAGEMAP PageMap)
{
	return MmFreePhysicalPage(MmPhysPageToPFN(PageMap));
}

static void MmpFreeVacantPageTables(uintptr_t Address)
{
	if (!MmCheckPteLocation(Address, false))
		// already freed
		return;
	
	// check if the page table this address' PTE is in is vacant
	PMMPTE PtePT = MmGetPteLocation(Address);
	PtePT = (PMMPTE)((uintptr_t)PtePT & ~(PAGE_SIZE - 1));
	for (int i = 0; i < 1024; i++)
	{
		if (PtePT[i] != 0)
			// isn't vacant
			return;
	}
	
	PMMPTE PtePD = MmGetPteLocation(MI_PTE_LOC(Address));
	MMPFN Pfn = MmPhysPageToPFN(*PtePD & MM_PTE_ADDRESSMASK);
	*PtePD = 0;
	MmFreePhysicalPage(Pfn);
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

bool MiMapAnonPage(uintptr_t Address, uintptr_t Permissions, bool NonPaged)
{
	PMMPTE Pte = MmGetPteLocationCheck(Address, true);
	return MmpMapSingleAnonPageAtPte(Pte, Permissions, NonPaged);
}

bool MiMapPhysicalPage(uintptr_t PhysicalPage, uintptr_t Address, uintptr_t Permissions)
{
	PMMPTE Pte = MmGetPteLocationCheck(Address, true);
	if (!Pte)
		return false;
	
	*Pte = (PhysicalPage & MM_PTE_ADDRESSMASK) | Permissions | MM_PTE_PRESENT;
	return true;
}

void MiUnmapPages(uintptr_t Address, size_t LengthPages)
{
	// Step 1. Unset the PRESENT bit on all pages in the range.
	for (size_t i = 0; i < LengthPages; i++)
	{
		PMMPTE pPTE = MmGetPteLocationCheck(Address + i * PAGE_SIZE, false);
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
		PMMPTE pPTE = MmGetPteLocationCheck(Address + i * PAGE_SIZE, false);
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
}

uintptr_t MiGetTopOfPoolManagedArea()
{
	return MI_GLOBAL_AREA_START << 22;
}

uintptr_t MiGetTopOfSecondPoolManagedArea()
{
	return MI_GLOBAL_AREA_START_2ND << 22;
}

bool MiMapAnonPages(uintptr_t Address, size_t SizePages, uintptr_t Permissions, bool NonPaged)
{
	// As an optimization, we'll wait until the PML1 index rolls over to zero before reloading the PTE pointer.
	uint64_t CurrentPml1 = PML1_IDX(Address);
	size_t DonePages = 0;
	
	PMMPTE PtePtr = MmGetPteLocationCheck(Address, true);
	
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
			PtePtr = MmGetPteLocationCheck(Address, true);
		}
	}
	
	// All allocations have succeeded! Let the caller know and don't undo our work. :)
	return true;
	
ROLLBACK:
	// Unmap all the pages that we have mapped.
	MiUnmapPages(Address, DonePages);
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
