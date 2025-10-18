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
	// UPDATE MmpAllocateFromPoolEntrySlab if changing these.

#ifdef IS_64_BIT
	#define POOL_ENTRY_COUNT 84
	// Size of a MIPOOL_ENTRY is 48.
	// Size of additional data is 32 bytes. (2xPtr=16 + 2xU64=16)
	// Note: Free space is 32 bytes
#else
	#define POOL_ENTRY_COUNT 64
	// Size of a MIPOOL_ENTRY is 32
	// Size of additional data is 24 bytes (2xPtr=8 + 2xU64=16)
	// Note: Free space is 8 bytes
#endif

	MIPOOL_ENTRY Entries[POOL_ENTRY_COUNT];
	LIST_ENTRY ListEntry;
	uint64_t Bitmap[2];
}
MIPOOL_ENTRY_SLAB, *PMIPOOL_ENTRY_SLAB;

static_assert(sizeof(MIPOOL_ENTRY_SLAB) <= PAGE_SIZE);
static_assert((sizeof(MIPOOL_ENTRY) & 0x7) == 0);

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
	
	const uint64_t FirstTwentyBitsSet = (1ULL << (POOL_ENTRY_COUNT - 64)) - 1;
	if ((Slab->Bitmap[1] & FirstTwentyBitsSet) != FirstTwentyBitsSet)
	{
		for (int i = 0, j = 64; i < POOL_ENTRY_COUNT - 64; i++, j++)
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
		#ifdef MIPOOL_DUMMY_SIGNATURE
			PoolEntry->Dummy = MIPOOL_DUMMY_SIGNATURE;
		#endif
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
	if (PoolEntry)
	{
		memset(PoolEntry, 0, sizeof *PoolEntry);
	#ifdef MIPOOL_DUMMY_SIGNATURE
		PoolEntry->Dummy = MIPOOL_DUMMY_SIGNATURE;
	#endif
	}
	
	KeReleaseSpinLock(&MmpPoolSlabListLock, Ipl);
	
	return PoolEntry;
}

void MiDeletePoolEntry(PMIPOOL_ENTRY Entry)
{
#ifdef MIPOOL_DUMMY_SIGNATURE
	ASSERT(Entry->Dummy == MIPOOL_DUMMY_SIGNATURE);
	
	ASSERT(Entry->ListEntry.Flink == NULL);
	ASSERT(Entry->ListEntry.Blink == NULL);
#endif
	
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
