/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/poolhdr.c
	
Abstract:
	This module implements the pool header provider.  Its only
	job is to provide pool headers.  It returns them from physical
	memory directly.
	
Author:
	iProgramInCpp - 27 November 2023
***/
#include "mi.h"

typedef struct
{
	// Size of a MIPOOL_ENTRY is 48.
	// UPDATE MmpAllocateFromPoolEntrySlab if changing this.
	MIPOOL_ENTRY Entries[84];
	LIST_ENTRY ListEntry;
	uint64_t Bitmap[2];
	
	// Note - Free space: 32 bytes
}
MIPOOL_ENTRY_SLAB, *PMIPOOL_ENTRY_SLAB;

static_assert(sizeof(MIPOOL_ENTRY_SLAB) <= PAGE_SIZE);

static LIST_ENTRY MmpPoolSlabList;
static KSPIN_LOCK MmpPoolSlabListLock;

PMIPOOL_ENTRY MmpAllocateFromPoolEntrySlab(PMIPOOL_ENTRY_SLAB Slab)
{
	if (Slab->Bitmap[0] != ~0ULL)
	{
		// Can allocate in the first part
		for (int i = 0; i < 64; i++)
		{
			if (~Slab->Bitmap[0] & (1ULL << i))
			{
				Slab->Bitmap[0] |=  1ULL << i;
				return &Slab->Entries[i];
			}
		}
	}
	
	const uint64_t FirstTwentyBitsSet = (1ULL << 20) - 1;
	if ((Slab->Bitmap[1] & FirstTwentyBitsSet) != FirstTwentyBitsSet)
	{
		for (int i = 0, j = 64; i < 20; i++, j++)
		{
			if (~Slab->Bitmap[1] & (1ULL << i))
			{
				Slab->Bitmap[1] |=  1ULL << i;
				return &Slab->Entries[j];
			}
		}
	}
	
	// No space left
	return NULL;
}

INIT
void MiInitPoolEntryAllocator()
{
	InitializeListHead(&MmpPoolSlabList);
}

PMIPOOL_ENTRY MiCreatePoolEntry()
{
	KIPL Ipl;
	KeAcquireSpinLock(&MmpPoolSlabListLock, &Ipl);
	
	PLIST_ENTRY Entry = MmpPoolSlabList.Flink;
	while (Entry != &MmpPoolSlabList)
	{
		PMIPOOL_ENTRY_SLAB Item = CONTAINING_RECORD(Entry, MIPOOL_ENTRY_SLAB, ListEntry);
		
		PMIPOOL_ENTRY PoolEntry = MmpAllocateFromPoolEntrySlab(Item);
		
		if (PoolEntry)
		{
			memset(PoolEntry, 0, sizeof *PoolEntry);
			KeReleaseSpinLock(&MmpPoolSlabListLock, Ipl);
			return PoolEntry;
		}
		
		Entry = Entry->Flink;
	}
	
	// Have to allocate a new entry.
	int Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
	{
		DbgPrint("MmpSlabContainerAllocate: Run out of memory! What will we do?!");
		KeCrash("x");
		// TODO: invoke the out of memory handler here, then try again
		KeReleaseSpinLock(&MmpPoolSlabListLock, Ipl);
		return NULL;
	}
	
	PMIPOOL_ENTRY_SLAB Item = MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn));
	memset(Item, 0, sizeof *Item);
	
	// Add it to the list.
	InsertHeadList(&MmpPoolSlabList, &Item->ListEntry);
	
	// Attempt to allocate as usual.
	PMIPOOL_ENTRY PoolEntry = MmpAllocateFromPoolEntrySlab(Item);
	
	KeReleaseSpinLock(&MmpPoolSlabListLock, Ipl);
	
	return PoolEntry;
}

void MiDeletePoolEntry(PMIPOOL_ENTRY Entry)
{
	// Get the containing slab.
	PMIPOOL_ENTRY_SLAB Slab = (PMIPOOL_ENTRY_SLAB) ((uintptr_t) Entry & ~(PAGE_SIZE - 1));
	
	// Get the index into the containing slab of the entry.
	int Index = (int)(Entry - Slab->Entries);
	
	// Clear the relevant bitmap.
	Slab->Bitmap[Index / 64] &= ~(1ULL << (Index % 64));
	
	// Clear the entry.
	memset(Entry, 0, sizeof *Entry);
	
	// If the bitmap is fully empty, remove the slab.
	if (Slab->Bitmap[0] == 0 && Slab->Bitmap[1] == 0)
	{
		RemoveEntryList(&Slab->ListEntry);
		
		int Pfn = MmPhysPageToPFN(MmGetHHDMOffsetFromAddr(Slab));
		
		MmFreePhysicalPage(Pfn);
	}
}
