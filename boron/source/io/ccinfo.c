/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	io/ccinfo.c

Abstract:
	This module implements functions regarding the I/O
	Cache Info structure.

Author:
	iProgramInCpp - 12 August 2026
***/
#include <io.h>
#include <ke.h>
#include <mm.h>

void IoInitializeCacheInfo(PIO_CACHE_INFO CacheInfo)
{
	MmInitializeCcb(&CacheInfo->PageCache);
	ExInitializeFastMutex(&CacheInfo->ViewCacheMutex);
	ExInitializeFastMutex(&CacheInfo->ModBitmapMutex);
	CacheInfo->ModBitmap = NULL;
	CacheInfo->BlockSize = 0;
}

void IoTeardownCacheInfo(PIO_CACHE_INFO CacheInfo)
{
	MmTearDownCcb(&CacheInfo->PageCache);
	IoSetBlockSizeCacheInfo(CacheInfo, 0);
}

bool IoSetBlockSizeCacheInfo(PIO_CACHE_INFO CacheInfo, size_t BlockSize)
{
	if (BlockSize == CacheInfo->BlockSize)
		return true;

	if (BlockSize != 0 && BlockSize < PAGE_SIZE / 64)
	{
		// The minimum depends on page size.  This is because IoGetModifiedMaskPerPageCacheInfo
		// uses a 64-bit bitmask.
		//
		// Examples:
		// 4KB pages: minimum 64 bytes.
		// 16KB pages: minimum 256 bytes.
		DbgPrint(
			"IoSetBlockSizeCacheInfo: Cannot set block size to %zu: minimum is %zu.",
			BlockSize,
			PAGE_SIZE / 64
		);
		return false;
	}

	// If the block size is less than the page size, and not 0, then
	PMMSLA NewSla = NULL;
	if (BlockSize != 0 && BlockSize < PAGE_SIZE)
	{
		NewSla = MmAllocatePool(POOL_NONPAGED, sizeof(MMSLA));
		if (!NewSla)
		{
			DbgPrint("IoSetBlockSizeCacheInfo: Cannot allocate mod bitmap. Out of memory");
			return false;
		}

		MmInitializeSlaEx(NewSla, 0, 0);
	}

	if (CacheInfo->ModBitmap)
	{
		MmDeinitializeSla(CacheInfo->ModBitmap, NULL);
		MmFreePool(CacheInfo->ModBitmap);
	}

	CacheInfo->ModBitmap = NewSla;
	CacheInfo->BlockSize = BlockSize;
	return true;
}

// Note: This is NOT thread-safe!  (And probably doesn't need to be, either.)
bool IoSetBlockSizeFile(PFILE_OBJECT FileObject, size_t BlockSize)
{
	return IoSetBlockSizeCacheInfo(&FileObject->Fcb->CacheInfo, BlockSize);
}

bool IoModRangeModifiedCacheInfo(
	PIO_CACHE_INFO CacheInfo,
	uint64_t StartOffset,
	size_t Size,
	bool Set
)
{
	if (Size == 0)
		return true;

	if (!CacheInfo->ModBitmap)
		return true;

	unsigned BitsPerEntry = sizeof(MMSLA_ENTRY) * 8;
	uint64_t Start = StartOffset / BitsPerEntry;
	uint64_t End = (StartOffset + Size - 1) / BitsPerEntry;
	unsigned StartInEntry = StartOffset % BitsPerEntry;
	unsigned EndInEntry = (StartOffset + Size - 1) % BitsPerEntry;

	MMSLA_ENTRY Mask, Entry, Result;

	ExAcquireFastMutex(&CacheInfo->ModBitmapMutex, false);

	// If Start and End are inside the same entry.
	if (Start == End)
	{
		Mask = Size >= 64 ? (MMSLA_ENTRY)(~0ULL) : ((MMSLA_ENTRY)(1ULL << Size) - 1);
		Mask <<= StartInEntry;

		Entry = MmLookUpEntrySla(CacheInfo->ModBitmap, Start);
		if (!Set && Entry == 0)
		{
			// Already cleared.
			ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
			return true;
		}

		if (Set)
			Entry |= Mask;
		else
			Entry &= ~Mask;

		Result = MmAssignEntrySla(CacheInfo->ModBitmap, Start, Entry);
		if (Result == 0 && Set)
		{
			// Out of memory.
			//
			// Modifying with clear enabled shouldn't throw out of memory, because
			// the entry is already allocated, and also 0 is a valid return value for
			// when an entry is fully cleared and there are no more modified blocks
			// for that specific SLA entry.
			DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (one block range)");
			ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
			return false;
		}

		ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
		return true;
	}

	// Start and End are not the same.
	Mask = (MMSLA_ENTRY)(~0ULL << StartInEntry);

	Entry = MmLookUpEntrySla(CacheInfo->ModBitmap, Start);

	if (Set)
		Entry |= Mask;
	else
		Entry &= ~Mask;

	Result = MmAssignEntrySla(CacheInfo->ModBitmap, Start, Entry);
	if (Result == 0 && Set)
	{
		// Out of memory.
		DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (longer range, setting start)");
		ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
		return false;
	}

	for (uint64_t i = Start + 1; i < End; i++)
	{
		Result = MmAssignEntrySla(CacheInfo->ModBitmap, i, Set ? (MMSLA_ENTRY)(~0ULL) : 0);
		if (Result == 0 && Set)
		{
			DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (longer range, setting middle)");
			ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
			return false;
		}
	}

	Mask = EndInEntry == 63 ? (MMSLA_ENTRY)(~0ULL) : ((MMSLA_ENTRY)(1ULL << (EndInEntry + 1)) - 1);

	Entry = MmLookUpEntrySla(CacheInfo->ModBitmap, End);

	if (Set)
		Entry |= Mask;
	else
		Entry &= ~Mask;

	Result = MmAssignEntrySla(CacheInfo->ModBitmap, End, Entry);
	if (Result == 0 && Set)
	{
		// Out of memory.
		DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (longer range, setting end)");
		ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
		return false;
	}

	ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);
	return true;
}

bool IoSetRangeModifiedCacheInfo(PIO_CACHE_INFO CacheInfo, uint64_t StartOffset, size_t Size)
{
	return IoModRangeModifiedCacheInfo(CacheInfo, StartOffset, Size, true);
}

void IoClearRangeModifiedCacheInfo(PIO_CACHE_INFO CacheInfo, uint64_t StartOffset, size_t Size)
{
	UNUSED bool Result;
	Result = IoModRangeModifiedCacheInfo(CacheInfo, StartOffset, Size, false);
	ASSERT(Result && "How come this failed!?");
}

bool IopIsBlockModified(
	PIO_CACHE_INFO CacheInfo,
	uint64_t BlockIndex,
	PMMSLA_ENTRY EntryCacheInOut,
	uint64_t* LastAccessedIndexInOut
)
{
	uint64_t SlaIndex = BlockIndex / (sizeof(MMSLA_ENTRY) * 8);
	unsigned InSlaIndex = BlockIndex % (sizeof(MMSLA_ENTRY) * 8);
	if (*LastAccessedIndexInOut != SlaIndex)
	{
		*LastAccessedIndexInOut = SlaIndex;
		*EntryCacheInOut = MmLookUpEntrySla(CacheInfo->ModBitmap, SlaIndex);
	}

	return *EntryCacheInOut & ((MMSLA_ENTRY)1ULL << InSlaIndex);
}

uint64_t IoGetModifiedMaskPerPageCacheInfo(
	PIO_CACHE_INFO CacheInfo,
	uint64_t PageNumberOffset,
	uint64_t* OutBitsetAll
)
{
	if (CacheInfo->BlockSize == 0)
		return ~0ULL;

	size_t BlocksPerPage = PAGE_SIZE / CacheInfo->BlockSize;

	uint64_t Mask = 0;
	uint64_t Start = PageNumberOffset * BlocksPerPage;
	uint64_t End = (PageNumberOffset + 1) * BlocksPerPage;

	ExAcquireFastMutex(&CacheInfo->ModBitmapMutex, false);

	uint64_t Current = 1, All = 0;
	MMSLA_ENTRY EntryCache = 0;
	uint64_t LastAccessedIndex = ~0ULL;
	for (uint64_t i = Start; i < End; i++)
	{
		if (IopIsBlockModified(CacheInfo, i, &EntryCache, &LastAccessedIndex)) {
			Mask |= Current;
		}

		All |= Current;
		Current <<= 1;
	}

	ExReleaseFastMutex(&CacheInfo->ModBitmapMutex);

	if (OutBitsetAll) {
		*OutBitsetAll = All;
	}

	return Mask;
}
