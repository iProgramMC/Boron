/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/sla.c
	
Abstract:
	This module implements the sparse linear array data structure.
	
	This data structure is used in the page cache, in anonymous
	(non-file-backed) memory sections, as well as in copy-on-write
	cels.
	
Author:
	iProgramInCpp - 7 December 2025
***/
#include "mi.h"

#define SLA_ENTRIES_PER_PAGE (PAGE_SIZE / sizeof(MMSLA_ENTRY))
#define PFN_ENTRIES_PER_PAGE (PAGE_SIZE / sizeof(MMPFN))

void MmInitializeSla(PMMSLA Sla)
{
	for (int i = 0; i < MM_SLA_DIRECT_ENTRY_COUNT; i++)
		Sla->Direct[i] = MM_SLA_NO_DATA;
	
	for (int i = 0; i < MM_SLA_INDIRECT_LEVELS; i++)
		Sla->Indirect[i] = PFN_INVALID;
}

void MmDeinitializeSla(PMMSLA Sla, MM_SLA_FREE_ENTRY_FUNC FreeEntryFunc)
{
	DbgPrint("TODO: MmDeinitializeSla(%p, %p)", Sla, FreeEntryFunc);
}

static MMPFN MmpAllocatePhysicalPageSlaInitialized()
{
	MMPFN Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
		return Pfn;
	
	MmBeginUsingHHDM();
	void* Page = MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn));
	memset(Page, 0xFF, PAGE_SIZE);
	MmEndUsingHHDM();
	return Pfn;
}

//
// Modify - Whether this call is used to modify the entry or simply read it.
//
// NewValue - The new value of the entry, if Modify is set to true. Otherwise, ignored.
//
// OutPrototypePtePointer - The prototype PTE pointer used for the page cache, generated
//   if Modify is set to true. Otherwise, ignored.
//
// EntryReferenceFunc - The function called when referencing this entry, called if Modify
//   is set to false. Otherwise, ignored.
//
static MMSLA_ENTRY MmpLookUpEntrySlaPlusModify(
	PMMSLA Sla,
	uint64_t EntryIndex,
	bool Modify,
	MMSLA_ENTRY NewValue,
	PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer,
	MM_SLA_REFERENCE_ENTRY_FUNC EntryReferenceFunc
)
{
	// Direct Lookup
	if (EntryIndex < MM_SLA_DIRECT_ENTRY_COUNT)
	{
		if (Modify)
		{
			Sla->Direct[EntryIndex] = NewValue;
			
			if (OutPrototypePtePointer)
			{
			#ifdef IS_32_BIT
				*OutPrototypePtePointer = MM_VIRTUAL_PROTO_PTE_PTR(&Sla->Direct[EntryIndex]);
			#else
				*OutPrototypePtePointer = (MM_PROTOTYPE_PTE_PTR) &Sla->Direct[EntryIndex];
			#endif
			}
		}
		else if (EntryReferenceFunc)
		{
			EntryReferenceFunc(&Sla->Direct[EntryIndex]);
		}
		
		return Sla->Direct[EntryIndex];
	}
	
	uint64_t CurrentIndex = EntryIndex - MM_SLA_DIRECT_ENTRY_COUNT;
	uint64_t MaxOffset = SLA_ENTRIES_PER_PAGE;
	for (int Level = 0; Level < MM_SLA_INDIRECT_LEVELS; Level++)
	{
		if (CurrentIndex >= MaxOffset)
		{
			// Need to move on to the next level.
			CurrentIndex -= MaxOffset;
			MaxOffset *= PFN_ENTRIES_PER_PAGE;
			continue;
		}
		
		// Navigate the chain up until the last level.
		//
		// Levels up until the last level have PFN_ENTRIES_PER_PAGE entries, because
		// they only store PFNs, however the last level stores SLA_ENTRIES_PER_PAGE
		// entries.  This distinction is important on 64-bit platforms where PFNs are
		// still 32-bit (it was decided that Boron would not run on systems with more
		// than 16 TB of RAM), but SLA entries are a full pointer size.
		MMPFN CurrentPfn = Sla->Indirect[Level];
		if (CurrentPfn == PFN_INVALID)
		{
			if (!Modify)
				return MM_SLA_NO_DATA;
			
			CurrentPfn = MmpAllocatePhysicalPageSlaInitialized();
			if (CurrentPfn == PFN_INVALID)
				return MM_SLA_OUT_OF_MEMORY;
			
			Sla->Indirect[Level] = CurrentPfn;
		}
		
		for (int i = 0; i < Level; i++)
		{
			MmBeginUsingHHDM();
			PMMPFN Entries = MmGetHHDMOffsetAddr(MmPFNToPhysPage(CurrentPfn));
			MMPFN NextPfn = Entries[CurrentIndex % PFN_ENTRIES_PER_PAGE];
			MmEndUsingHHDM();
			
			if (NextPfn == PFN_INVALID)
			{
				if (!Modify)
					return MM_SLA_NO_DATA;
				
				NextPfn = MmpAllocatePhysicalPageSlaInitialized();
				if (NextPfn == PFN_INVALID)
					return MM_SLA_OUT_OF_MEMORY;
				
				MmBeginUsingHHDM();
				PMMPFN Entries = MmGetHHDMOffsetAddr(MmPFNToPhysPage(CurrentPfn));
				Entries[CurrentIndex % PFN_ENTRIES_PER_PAGE] = NextPfn;
				MmEndUsingHHDM();
			}
			
			CurrentIndex /= PFN_ENTRIES_PER_PAGE;
			CurrentPfn = NextPfn;
		}
		
		MmBeginUsingHHDM();
		
		uintptr_t BaseAddress = MmPFNToPhysPage(CurrentPfn);
		PMMSLA_ENTRY Entries = MmGetHHDMOffsetAddr(BaseAddress);
		
		if (Modify)
		{
			Entries[CurrentIndex] = NewValue;
			
			if (OutPrototypePtePointer)
			{
			#ifdef IS_32_BIT
				*OutPrototypePtePointer = (MM_PROTOTYPE_PTE_PTR)(BaseAddress + (sizeof(MMSLA_ENTRY) * CurrentIndex));
			#else
				*OutPrototypePtePointer = &Entries[CurrentIndex];
			#endif
			}
		}
		else if (EntryReferenceFunc)
		{
			EntryReferenceFunc(&Entries[CurrentIndex]);
		}
		
		MMSLA_ENTRY Entry = Entries[CurrentIndex];
		MmEndUsingHHDM();
		
		return Entry;
	}
	
	DbgPrint("ERROR in MmLookUpEntrySla: Entry index %llu is too big.", EntryIndex);
	return MM_SLA_NO_DATA;
}

MMSLA_ENTRY MmLookUpEntrySlaEx(
	PMMSLA Sla,
	uint64_t EntryIndex,
	MM_SLA_REFERENCE_ENTRY_FUNC EntryReferenceFunc
)
{
	return MmpLookUpEntrySlaPlusModify(Sla, EntryIndex, false, 0, NULL, EntryReferenceFunc);
}

MMSLA_ENTRY MmAssignEntrySlaEx(
	PMMSLA Sla,
	uint64_t EntryIndex,
	MMSLA_ENTRY NewValue,
	PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer
)
{
	return MmpLookUpEntrySlaPlusModify(Sla, EntryIndex, true, NewValue, OutPrototypePtePointer, NULL);
}
