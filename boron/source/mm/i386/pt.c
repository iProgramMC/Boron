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

bool MmCheckPteLocationAllocator(
	uintptr_t Address,
	bool GenerateMissingLevels,
	MM_PAGE_ALLOCATOR_METHOD PageAllocate
)
{
	PMMPTE Pte;
	uintptr_t SupervisorBit;
	
	ASSERT(Address < MI_PML1_LOCATION || (uint64_t)Address >= MI_PML1_LOC_END);
	
	if (Address >= MM_KERNEL_SPACE_BASE)
		SupervisorBit = 0;
	else
		SupervisorBit = MM_PROT_USER;
	
	// Check the presence of the PT
	Pte = MmGetPteLocation(MI_PTE_LOC(Address));
	if (!MmIsPresentPte(*Pte))
	{
		if (!GenerateMissingLevels)
			return false;
		
		MMPFN PtAllocated = PageAllocate();
		if (PtAllocated == PFN_INVALID)
			return false;
		
		*Pte = MmBuildPte(PtAllocated, MM_PROT_READ | MM_PROT_WRITE | SupervisorBit);
	}
	
	// Page table exists.
	return true;
}

bool MmCheckPteLocation(uintptr_t Address, bool GenerateMissingLevels)
{
	return MmCheckPteLocationAllocator(Address, GenerateMissingLevels, MmAllocatePhysicalPage);
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
	MMPTE ZeroPte = MmBuildZeroPte();
	for (int i = 0; i < 512; i++)
		NewPageMappingAccess[i] = ZeroPte;
	
	// then copy out the kernel's latter 512 entries
	for (int i = 512; i < 1024; i++)
		NewPageMappingAccess[i] = OldPageDirectory[i];
	
	// and replace that last entry with the pointer to this one.
	NewPageMappingAccess[MI_RECURSIVE_PAGING_START] = MmBuildPte(
		NewPageMappingPFN,
		MM_PROT_READ | MM_PROT_WRITE
	);
	
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
	MMPTE ZeroPte = MmBuildZeroPte();
	PMMPTE PtePT = MmGetPteLocation(Address);
	PtePT = (PMMPTE)((uintptr_t)PtePT & ~(PAGE_SIZE - 1));
	for (int i = 0; i < 1024; i++)
	{
		if (!MmIsEqualPte(PtePT[i], ZeroPte))
			// isn't vacant
			return;
	}
	
	PMMPTE PtePD = MmGetPteLocation(MI_PTE_LOC(Address));
	MMPFN Pfn = MmGetPfnPte(*PtePD);
	*PtePD = ZeroPte;
	MmFreePhysicalPage(Pfn);
}

static bool MmpMapSingleAnonPageAtPte(PMMPTE Pte, uintptr_t Permissions, bool NonPaged)
{
	if (!Pte)
		return false;
	
    if (MM_DBG_NO_DEMAND_PAGING || NonPaged)
	{
		MMPFN pfn = MmAllocatePhysicalPage();
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
		
		*Pte = MmBuildPte(pfn, MM_MISC_IS_FROM_PMM | Permissions);
		return true;
	}
	
	(void) Permissions;
	*Pte = MmBuildAbsentPte(MM_PAGE_COMMITTED);
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
	
	*Pte = MmBuildPte(MmPhysPageToPFN(PhysicalPage), Permissions);
	return true;
}

void MiUnmapPages(uintptr_t Address, size_t LengthPages)
{
	MMPTE ZeroPte = MmBuildZeroPte();
	
	// Step 1. Unset the PRESENT bit on all pages in the range.
	for (size_t i = 0; i < LengthPages; i++)
	{
		PMMPTE pPTE = MmGetPteLocationCheck(Address + i * PAGE_SIZE, false);
		if (!pPTE)
			continue;
		
		if (MmIsPresentPte(*pPTE)) {
			*pPTE = MmBuildWasPresentPte(*pPTE);
		}
		else {
			*pPTE = ZeroPte;
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
		
		if (MmIsFromPmmPte(*pPTE))
		{
			MMPFN Pfn = MmGetPfnPte(*pPTE);
			MmFreePhysicalPage(Pfn);
			*pPTE = ZeroPte;
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

uintptr_t MmGetPteBitsFromProtection(int Protection)
{
	uintptr_t Bits = 0;
	if (Protection & PAGE_READ)
		Bits |= MM_PROT_READ;
	if (Protection & PAGE_WRITE)
		Bits |= MM_PROT_WRITE;
	if (Protection & PAGE_EXECUTE)
		Bits |= MM_PROT_EXEC;
	
	return Bits;
}
