/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/cache.c
	
Abstract:
	This module implements the cache control block (CCB)
	object for the memory manager.
	
Author:
	iProgramInCpp - 18 August 2024
***/
#include "mi.h"

void MmInitializeCcb(PCCB Ccb)
{
	memset(Ccb, 0, sizeof *Ccb);
	
	KeInitializeMutex(&Ccb->Mutex, MM_CCB_MUTEX_LEVEL);
}

PCCB_ENTRY MmGetEntryPointerCcb(PCCB Ccb, uint64_t PageOffset, bool TryAllocateLowerLevels)
{
	ASSERT(Ccb->Mutex.Header.Signaled > 0 && Ccb->Mutex.OwnerThread == KeGetCurrentThread());
	
	if (PageOffset < MM_DIRECT_PAGE_COUNT)
		return &Ccb->Direct[PageOffset];
	PageOffset -= MM_DIRECT_PAGE_COUNT;
	
	size_t EntriesPerPage = ARRAY_COUNT(Ccb->Level1Indirect[0].Entries);
	size_t Range = EntriesPerPage;
	
	// Level 1
	if (PageOffset < EntriesPerPage)
	{
		//DbgPrint("L1: OffsetL0=%04x", PageOffset);
		if (!Ccb->Level1Indirect)
		{
			if (!TryAllocateLowerLevels)
				return NULL;
			
			if (!(Ccb->Level1Indirect = MmAllocatePhysicalPageHHDM()))
			{
				DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): could not allocate 1st level indirection table", Ccb, PageOffset, TryAllocateLowerLevels);
				return NULL;
			}
		}
		
		return &Ccb->Level1Indirect->Entries[PageOffset];
	}
	PageOffset -= EntriesPerPage;
	Range *= EntriesPerPage;
	
	// Level 2
	if (PageOffset < Range)
	{
		uint64_t OffsetL1 = PageOffset % EntriesPerPage;
		uint64_t OffsetL0 = PageOffset / EntriesPerPage;
		//DbgPrint("L2: OffsetL0=%04x OffsetL1=%04x", OffsetL0, OffsetL1);
		
		if (!Ccb->Level2Indirect)
		{
			if (!TryAllocateLowerLevels)
				return NULL;
			
			if (!(Ccb->Level2Indirect = MmAllocatePhysicalPageHHDM()))
			{
				DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): could not allocate 2nd level (root) indirection table", Ccb, PageOffset, TryAllocateLowerLevels);
				return NULL;
			}
		}
		if (!Ccb->Level2Indirect->Entries[OffsetL0].Indirection)
		{
			if (!TryAllocateLowerLevels)
				return NULL;
			
			if (!(Ccb->Level2Indirect->Entries[OffsetL0].Indirection = MmAllocatePhysicalPageHHDM()))
			{
				DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): could not allocate 2nd level (L1) indirection table", Ccb, PageOffset, TryAllocateLowerLevels);
				return NULL;
			}
		}
		
		return &Ccb->Level2Indirect->Entries[OffsetL0].Indirection->Entries[OffsetL1];
	}
	PageOffset -= Range;
	Range *= EntriesPerPage;
	
	// Level 3
	if (PageOffset < Range)
	{
		uint64_t OffsetL2 = PageOffset % EntriesPerPage;
		uint64_t OffsetL1 = PageOffset / EntriesPerPage % EntriesPerPage;
		uint64_t OffsetL0 = PageOffset / EntriesPerPage / EntriesPerPage;
		//DbgPrint("L3: OffsetL0=%04x OffsetL1=%04x OffsetL2=%04x", OffsetL0, OffsetL1, OffsetL2);
		
		if (!Ccb->Level3Indirect)
		{
			if (!TryAllocateLowerLevels)
				return NULL;
			
			if (!(Ccb->Level3Indirect = MmAllocatePhysicalPageHHDM()))
			{
				DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): could not allocate 3rd level (root) indirection table", Ccb, PageOffset, TryAllocateLowerLevels);
				return NULL;
			}
		}
		if (!Ccb->Level3Indirect->Entries[OffsetL0].Indirection)
		{
			if (!TryAllocateLowerLevels)
				return NULL;
			
			if (!(Ccb->Level3Indirect->Entries[OffsetL0].Indirection = MmAllocatePhysicalPageHHDM()))
			{
				DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): could not allocate 3rd level (L1) indirection table", Ccb, PageOffset, TryAllocateLowerLevels);
				return NULL;
			}
		}
		
		if (!Ccb->Level3Indirect->Entries[OffsetL0].Indirection->Entries[OffsetL1].Indirection)
		{
			if (!TryAllocateLowerLevels)
				return NULL;
			
			if (!(Ccb->Level3Indirect->Entries[OffsetL0].Indirection->Entries[OffsetL1].Indirection = MmAllocatePhysicalPageHHDM()))
			{
				DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): could not allocate 3rd level (L1) indirection table", Ccb, PageOffset, TryAllocateLowerLevels);
				return NULL;
			}
		}
		
		return &Ccb->Level3Indirect->Entries[OffsetL0].Indirection->Entries[OffsetL1].Indirection->Entries[OffsetL2];
	}
	PageOffset -= Range;
	Range *= EntriesPerPage;
	
	// Level 4
	if (PageOffset < Range)
	{
		DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): TODO: level 4", Ccb, PageOffset, TryAllocateLowerLevels);
		return NULL;
	}
	PageOffset -= Range;
	Range *= EntriesPerPage;
	
#if MM_INDIRECTION_LEVELS == 5
	// Level 5
	if (PageOffset < Range)
	{
		DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): TODO: level 5", Ccb, PageOffset, TryAllocateLowerLevels);
		return NULL;
	}
	PageOffset -= Range;
	Range *= EntriesPerPage;
#endif
	
	DbgPrint("MmGetEntryPointerCcb(%p, %llu, %d): no more levels to check", Ccb, PageOffset, TryAllocateLowerLevels);
	return NULL;
}
