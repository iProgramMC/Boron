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

#ifdef IS_64_BIT

PMIPOOL_ENTRY_SLAB MiAllocatePoolHeaderSlab()
{
	MMPFN Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
	{
		DbgPrint("WARNING in MiAllocatePoolHeaderSlab: No physical memory left!");
		return NULL;
	}
	
	// HHDM is supported on 64-bit, so just return this
	return MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn));
}

void MiFreePoolHeaderSlab(PMIPOOL_ENTRY_SLAB Address)
{
	MMPFN Pfn = MmPhysPageToPFN(MmGetHHDMOffsetFromAddr(Address));
	MmFreePhysicalPage(Pfn);
}

#else

#define POOL_HDR_MAX_PFNS_MAPPED (MI_POOL_HEADERS_SIZE / PAGE_SIZE)

static KSPIN_LOCK MiPoolHeaderMapLock;
static uint32_t   MiPoolHeaderPfnsMappedBitmap[POOL_HDR_MAX_PFNS_MAPPED / 32];
static_assert(ARRAY_COUNT(MiPoolHeaderPfnsMappedBitmap) == 64);

PMIPOOL_ENTRY_SLAB MiAllocatePoolHeaderSlab()
{
	MMPFN Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
	{
		DbgPrint("WARNING in MiAllocatePoolHeaderSlab: No physical memory left!");
		return NULL;
	}
	
	KIPL Ipl;
	KeAcquireSpinLock(&MiPoolHeaderMapLock, &Ipl);
	
	// Find a place in the bitmap
	const size_t NotFound = 0xFFFFFFFF;
	size_t IndexFound = NotFound;
	
	for (size_t i = 0; i < POOL_HDR_MAX_PFNS_MAPPED / 32 && IndexFound == NotFound; i++)
	{
		if (MiPoolHeaderPfnsMappedBitmap[i] == 0xFFFFFFFF)
			continue;
		
		// claim the first clear bit
		for (int k = 0; k < 32; k++)
		{
			if (~MiPoolHeaderPfnsMappedBitmap[i] & (1 << k))
			{
				MiPoolHeaderPfnsMappedBitmap[i] |= 1 << k;
				IndexFound = i * 32 + k;
				break;
			}
		}
	}
	
	if (IndexFound == NotFound)
	{
		DbgPrint("WARNING in MiAllocatePoolHeaderSlab: No more space left!");
		KeReleaseSpinLock(&MiPoolHeaderMapLock, Ipl);
		MmFreePhysicalPage(Pfn);
		return NULL;
	}
	
	void* Address = (void*)(MI_POOL_HEADERS_START + IndexFound * PAGE_SIZE);
	
	PMMPTE Pte = (PMMPTE) MI_PTE_LOC((uintptr_t) Address);
	*Pte = MmPFNToPhysPage(Pfn) | MM_PTE_PRESENT | MM_PTE_READWRITE;
	KeInvalidatePage(Pte);
	
	KeReleaseSpinLock(&MiPoolHeaderMapLock, Ipl);
	return Address;
}

void MiFreePoolHeaderSlab(PMIPOOL_ENTRY_SLAB Address)
{
	KIPL Ipl;
	KeAcquireSpinLock(&MiPoolHeaderMapLock, &Ipl);
	
	PMMPTE Pte = (PMMPTE) MI_PTE_LOC((uintptr_t) Address);
	MMPFN Pfn = MmPhysPageToPFN(*Pte & MM_PTE_ADDRESSMASK);
	
	// clear the PTE
	*Pte = 0;
	KeInvalidatePage(Address);
	
	// then free
	MmFreePhysicalPage(Pfn);
	
	// and mark as unmapped
	uint32_t Index = ((uintptr_t)Address - MI_POOL_HEADERS_START) / PAGE_SIZE;
	uint32_t BIndex = Index >> 5, BSubIndex = Index & 0x1F;
	
	ASSERT(MiPoolHeaderPfnsMappedBitmap[BIndex] & (1 << BSubIndex));
	MiPoolHeaderPfnsMappedBitmap[BIndex] &= ~(1 << BSubIndex);
	
	KeReleaseSpinLock(&MiPoolHeaderMapLock, Ipl);
}

#endif

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
	PMIPOOL_ENTRY_SLAB Item = MiAllocatePoolHeaderSlab();
	if (!Item)
	{
		KeReleaseSpinLock(&MmpPoolSlabListLock, Ipl);
		return NULL;
	}
	
	memset(Item, 0, sizeof *Item);
	
	// Add it to the list.
	InsertHeadList(&MmpPoolSlabList, &Item->ListEntry);
	
	// Attempt to allocate as usual.  This should succeed,
	// because we just freshly allocated a new entry.
	PMIPOOL_ENTRY PoolEntry = MmpAllocateFromPoolEntrySlab(Item);
	ASSERT(PoolEntry);
	
	memset(PoolEntry, 0, sizeof *PoolEntry);
#ifdef MIPOOL_DUMMY_SIGNATURE
	PoolEntry->Dummy = MIPOOL_DUMMY_SIGNATURE;
#endif
	
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
		MiFreePoolHeaderSlab(Slab);
	}
}
