/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/arm/pt.c
	
Abstract:
	This module implements page table management for
	the arm platform.
	
Author:
	iProgramInCpp - 25 December 2025
***/

#include <main.h>
#include <string.h>
#include <ke.h>
#include <arch.h>
#include "../mi.h"

static MMPTE MmBuildL1Pte(uintptr_t Address)
{
	MMPTE Pte;
	MmHardwarePte(Pte) = (Address & ~0x3FF) | MM_ARM_PTEL1_COARSE_PAGE_TABLE;
	return Pte;
}

extern char KiExceptionHandlerTable[]; // NOTE: This is a *PHYSICAL* address!

HPAGEMAP MiCreatePageMapping()
{
	MMPFN NewPageMapping = MmAllocatePhysicalContiguousRegion(4, 0x3FFF);
	if (NewPageMapping == PFN_INVALID)
	{
		DbgPrint("Error, can't create a new page mapping, because we can't create the first level.");
		return 0;
	}
	
	// This page is referenced in the L1 of the new page mapping.
	// It's done this way so that we can actually implement a self-referencing
	// page mapping (a.k.a. fractal page mapping), without hardware support for
	// it.
	MMPFN Jibbie = MmAllocatePhysicalPage();
	
	// Inside of Jibbie we place four PTEs: one which indexes the root page map
	// at no offset, one at +4K, one at +8K, and one at +12K. Then we also place
	// a reference to Jibbie inside of itself, and also to Debbie.
	
	// That's the L1 mapped, now we need to map the L2s.  There are 4096 PTEs,
	// where each of these PTEs matches up to a 1MB section, and each L2 page
	// table is 1KB in size. So really, Boron will treat it as 1024 PTEs that
	// point to 4KB L2 page tables. (these logical PTEs are each equivalent to
	// FOUR different hardware L1 PTEs.)
	//
	// In order to map all 4 MB of L2 page tables, we need another page frame.
	MMPFN Debbie = MmAllocatePhysicalPage();
	
	if (Jibbie == PFN_INVALID || Debbie == PFN_INVALID)
	{
		if (Jibbie != PFN_INVALID)
			MmFreePhysicalPage(Jibbie);
		
		if (Debbie != PFN_INVALID)
			MmFreePhysicalPage(Debbie);
		
		MmFreePhysicalContiguousRegion(NewPageMapping, 4);
		DbgPrint("Error, can't create a new page mapping, because we can't allocate either Jibbie or Debbie.");
		return 0;
	}
	
	// Lock the kernel space's lock to not get any surprises.
	MmLockKernelSpaceShared();
	MmBeginUsingHHDM();
	
	PMMPTE JibbiePtr = MmGetHHDMOffsetAddr(MmPFNToPhysPage(Jibbie));
	memset(JibbiePtr, 0, PAGE_SIZE);
	
	for (int i = 0; i < 4; i++)
		JibbiePtr[i] = MmBuildPte(NewPageMapping + i, MM_PROT_READ | MM_PROT_WRITE);
	
	JibbiePtr[4] = MmBuildPte(Jibbie, MM_MISC_IS_FROM_PMM | MM_PROT_READ | MM_PROT_WRITE);
	JibbiePtr[5] = MmBuildPte(Debbie, MM_MISC_IS_FROM_PMM | MM_PROT_READ | MM_PROT_WRITE);
	
	// The 1008th entry of Jibbie maps the exception handler pointers
	// (at 0xFFFF0000), so map it too.  Note that KiExceptionHandlerTable
	// is a physical address.
	JibbiePtr[1008] = MmBuildPte(MmPhysPageToPFN((uintptr_t) KiExceptionHandlerTable), MM_PROT_READ | MM_PROT_WRITE | MM_PROT_EXEC);
	
	PMMPTE RootPtr = MmGetHHDMOffsetAddr(MmPFNToPhysPage(NewPageMapping));
	PMMPTE OldRootPtr = (PMMPTE) MI_PML1_LOCATION;
	memset(RootPtr, 0, PAGE_SIZE * 4);
	
	// Copy the higher half from the old root to the new root.
	for (int i = 2048; i < 4096; i++) {
		RootPtr[i] = OldRootPtr[i];
	}
	
	// And also, to Debbie.
	PMMPTE DebbiePtr = MmGetHHDMOffsetAddr(MmPFNToPhysPage(Debbie));
	memset(DebbiePtr, 0, PAGE_SIZE);
	
	MMPTE ZeroPte = MmBuildZeroPte();
	for (int i = 512; i < 1024; i++) {
		if (!MmIsEqualPte(OldRootPtr[i * 4], ZeroPte)) {
			MMPFN Pfn = MmGetPfnPte(OldRootPtr[i * 4]);
			DebbiePtr[i] = MmBuildPte(Pfn, MM_PROT_READ | MM_PROT_WRITE);
		}
	}
	
	// Replace the last few entries with pointers to Jibbie and Debbie.
	uintptr_t JibbieAddress = Jibbie * PAGE_SIZE;
	uintptr_t DebbieAddress = Debbie * PAGE_SIZE;
	for (int i = 0; i < 4; i++) {
		RootPtr[4088 + i] = MmBuildL1Pte(DebbieAddress + i * 1024);
		RootPtr[4092 + i] = MmBuildL1Pte(JibbieAddress + i * 1024);
	}
	
	MmEndUsingHHDM();
	MmFlushTlbUpdates();
	MmUnlockKernelSpace();
	
	DbgPrint("ROOT: %p", NewPageMapping*PAGE_SIZE);
	DbgPrint("Jibbie: %p", Jibbie*PAGE_SIZE);
	DbgPrint("Debbie: %p", Debbie*PAGE_SIZE);
	
	uintptr_t NewPageMappingResult = MmPFNToPhysPage (NewPageMapping);
	return (HPAGEMAP) NewPageMappingResult;
}

void MiFreePageMapping(HPAGEMAP PageMap)
{
	MMPFN Jibbie, Debbie, Root;
	
	Root = MmPhysPageToPFN(PageMap);
	
	MmBeginUsingHHDM();
	PMMPTE RootPtr = MmGetHHDMOffsetAddr(PageMap);
	
	// this works because MM_PTE_PFN fetches bits [31..12], and the root entries
	// have the coarse page table address from bits [31..10]
	Debbie = MmGetPfnPte(RootPtr[4088]);
	Jibbie = MmGetPfnPte(RootPtr[4092]);
	
	MmEndUsingHHDM();
	
	MmFreePhysicalPage(Jibbie);
	MmFreePhysicalPage(Debbie);
	MmFreePhysicalContiguousRegion(Root, 4);
	MmFlushTlbUpdates();
}

bool MmCheckPteLocationAllocator(
	uintptr_t Address,
	bool GenerateMissingLevels,
	MM_PAGE_ALLOCATOR_METHOD PageAllocate
)
{
	ASSERT(Address < MI_PML1_LOCATION && "MmCheckPteLocation for regions inside the L1 and L2 maps is unimplemented.");
	
	MMADDRESS_CONVERT Convert;
	Convert.Long = Address;
	
	uintptr_t IsFromPmmFlag = 0;
	if (PageAllocate == &MmAllocatePhysicalPage)
		IsFromPmmFlag = MM_MISC_IS_FROM_PMM;
	
	// check if the corresponding L1 page table is present through Jibbie.
	PMMPTE PtePtr = (PMMPTE) MI_PML1_LOCATION;
	if (!MmIsPresentPte(PtePtr[Convert.Level1Index]))
	{
		if (!GenerateMissingLevels)
			return false;
		
		int Index = Convert.Level1Index & ~3;
		MMPFN Pfn = PageAllocate();
		if (Pfn == PFN_INVALID) {
			DbgPrint("MmCheckPteLocation: Out of memory!");
			return false;
		}
		
		for (int i = 0; i < 4; i++) {
			PtePtr[Index + i] = MmBuildL1Pte(Pfn * PAGE_SIZE + i * 1024);
		}
		
		// Debbie must also be updated.
		PtePtr = (PMMPTE) MI_PML2_MIRROR_LOCATION;
		PtePtr[Convert.Level1Index >> 2] = MmBuildPte(Pfn, IsFromPmmFlag | MM_PROT_READ | MM_PROT_WRITE);
		MmFlushTlbUpdates();
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

PMMPTE MmGetPteLocation(uintptr_t Address)
{
	ASSERT(Address < MI_PML1_LOCATION && "MmGetPteLocation for regions inside the L1 and L2 maps is unimplemented.");
	
	PMMPTE PtePtr = (PMMPTE)MI_PTE_LOC(Address);
	
	// HACK: Instead of just invalidating everything in the function
	// MiFreeUnusedMappingLevelsInCurrentMap like I am supposed to,
	// I will invalidate the TLB here.
	//
	// I know this is bad, but come on, when are we *ever* going to
	// *not* go through this function?
	KeInvalidatePage(PtePtr);
	MmFlushTlbUpdates();
	
	return PtePtr;
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
			return false;
		}
		
		if (!Pte)
		{
			return false;
		}
		
		*Pte = MmBuildPte(pfn, MM_MISC_IS_FROM_PMM | Permissions);
		MmFlushTlbUpdates();
		return true;
	}
	
	(void) Permissions;
	*Pte = MmBuildAbsentPte(MM_PAGE_COMMITTED);
	MmFlushTlbUpdates();
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
	MmFlushTlbUpdates();
	return true;
}

void MiUnmapPages(uintptr_t Address, size_t LengthPages)
{
	MMPTE ZeroPte = MmBuildZeroPte();
	
	for (size_t i = 0; i < LengthPages; i++)
	{
		PMMPTE pPTE = MmGetPteLocationCheck(Address + i * PAGE_SIZE, false);
		if (!pPTE)
			continue;
		
		MMPTE Pte = *pPTE;
		*pPTE = ZeroPte;
		MmFlushTlbUpdates();
		
		if (MmIsPresentPte(Pte))
			MmFreePhysicalPage(MmGetPfnPte(Pte));
	}
	
	MmFlushTlbUpdates();
	MmIssueTLBShootDown(Address, LengthPages);
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
	uint64_t CurrentPtL2Index = PT_L2_IDX(Address);
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
		CurrentPtL2Index++;
		DonePages++;
		
		// Despite L2 page tables being 1KB in size, we still scroll through 1024 entries,
		// because this implementation of the page tables always allocates L2 page tables
		// in 4KB regions.
		if (CurrentPtL2Index % (PAGE_SIZE / sizeof(MMPTE)) == 0)
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
	MmFlushTlbUpdates();
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
