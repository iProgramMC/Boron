/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                          mapper.c                           *
\*************************************************************/
#include "mapint.h"
#include "phys.h"

#ifdef TARGET_AMD64

PPAGEMAP PageMap;

PPAGEMAP GetCurrentPageMap()
{
	uint64_t Return;
	ASM("mov %%cr3, %0":"=a"(Return));
	return (void*) (GetHhdmOffset() + Return);
}

uint64_t* PhysToInt64Array(uintptr_t PhysAddr)
{
	return (uint64_t*)(GetHhdmOffset() + PhysAddr);
}

void InitializeMapper()
{
	PageMap = GetCurrentPageMap();
	
	InitializePmm();
}

PMMPTE GetPTEPointer(PPAGEMAP Mapping, uintptr_t Address, bool AllocateMissingPMLs)
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
	
	PPAGEMAP CurrentLevel = Mapping;
	PMMPTE EntryPointer = NULL;
	
	for (int pml = 4; pml >= 1; pml--)
	{
		PMMPTE Entries = (PMMPTE) GetHhdmOffsetPtr((uintptr_t)CurrentLevel);
		
		EntryPointer = &Entries[indices[pml]];
		
		MMPTE Entry = *EntryPointer;
		
		if (pml > 1 && (~Entry & MM_PTE_PRESENT))
		{
			// not present!! Do we allocate it?
			if (!AllocateMissingPMLs)
				return NULL;
			
			uintptr_t MapAddress = AllocatePages(1, false);
			memset(GetHhdmOffsetPtr(MapAddress), 0, PAGE_SIZE);
			*EntryPointer = Entry = MM_PTE_PRESENT | MM_PTE_READWRITE | MapAddress;
		}
		
		if (pml > 1 && (Entry & MM_PTE_PAGESIZE))
		{
			// Higher page size, we can't allocate here.  Probably HHDM or something - the kernel itself doesn't use this
			//SLogMsg("GetPTEPointer: Address %p contains a higher page size, we don't support that for now", Address);
			return NULL;
		}
		
		//SLogMsg("DUMPING PML%d (got here from PML%d[%d]) :", pml, pml + 1, indices[pml + 1]);
		//for (int i = 0; i < 512; i++)
		//	SLogMsg("[%d] = %p", i, Entries[i]);
		
		CurrentLevel = (PPAGEMAP)(Entry & MM_PTE_ADDRESSMASK);
	}
	
	return EntryPointer;
}

static void MapSinglePageAtPte(PMMPTE Pte, uintptr_t Permissions, bool ReservedKernel)
{
	if (!Pte)
		return;
	
	uintptr_t Page = AllocatePages(1, ReservedKernel);
	
	*Pte = MM_PTE_PRESENT | Permissions | Page;
}

void MapInMemory(uintptr_t Address, size_t SizePages, uintptr_t Permissions, bool ReservedKernel)
{
	PPAGEMAP Mapping = PageMap;
	
	// As an optimization, we'll wait until the PML1 index rolls over to zero before reloading the PTE pointer.
	uint64_t CurrentPml1 = PML1_IDX(Address);
	size_t DonePages = 0;
	
	PMMPTE PtePtr = GetPTEPointer(Mapping, Address, true);
	
	for (size_t i = 0; i < SizePages; i++)
	{
		// If one of these fails, then we should roll back.
		MapSinglePageAtPte(PtePtr, Permissions, ReservedKernel);
		
		// Increase the address size, get the next PTE pointer, update the current PML1, and
		// increment the number of mapped pages (since this one was successfully mapped).
		Address += PAGE_SIZE;
		PtePtr++;
		CurrentPml1++;
		DonePages++;
		
		if (CurrentPml1 == 0)
		{
			// We have rolled over.
			PtePtr = GetPTEPointer(Mapping, Address, true);
		}
	}
}

#else
#error Hey
#endif
