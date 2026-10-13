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
	KeInitializeMutex(&CacheInfo->ViewCacheMutex, 0);

	KeInitializeMutex(&CacheInfo->ModBitmapMutex, 0);
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
	if (BlockSize == CacheInfo->BlockSize) {
		return true;
	}

	if (CacheInfo->ModBitmap)
	{
		MmDeinitializeSla(CacheInfo->ModBitmap, NULL);
		MmFreePool(CacheInfo->ModBitmap);
		CacheInfo->ModBitmap = NULL;
		CacheInfo->BlockSize = 0;
	}

	if (BlockSize == 0)
	{
		return true;
	}

	CacheInfo->ModBitmap = MmAllocatePool(POOL_NONPAGED, sizeof(MMSLA));
	if (!CacheInfo->ModBitmap)
	{
		DbgPrint("IoSetBlockSizeCacheInfo: Cannot allocate mod bitmap. Out of memory");
		return false;
	}

	MmInitializeSlaEx(CacheInfo->ModBitmap, 0, 0);
	CacheInfo->BlockSize = BlockSize;
	return true;
}

bool IoSetRangeModifiedCacheInfo(PIO_CACHE_INFO CacheInfo, uint64_t StartOffset, size_t Size)
{
	if (Size == 0)
		return true;

	unsigned BitsPerEntry = sizeof(MMSLA_ENTRY) * 8;
	uint64_t Start = StartOffset / BitsPerEntry;
	uint64_t End = (StartOffset + Size - 1) / BitsPerEntry;
	unsigned StartInEntry = StartOffset % BitsPerEntry;
	unsigned EndInEntry = (StartOffset + Size - 1) % BitsPerEntry;

	MMSLA_ENTRY Mask, Entry, Result;

	KeWaitForSingleObject(&CacheInfo->ModBitmapMutex, false, TIMEOUT_INFINITE, MODE_KERNEL);

	// If Start and End are inside the same entry.
	if (Start == End)
	{
		Mask = Size >= 64 ? (MMSLA_ENTRY)(~0ULL) : ((MMSLA_ENTRY)(1ULL << Size) - 1);
		Mask <<= StartInEntry;

		Entry = MmLookUpEntrySla(CacheInfo->ModBitmap, Start);
		Entry |= Mask;

		Result = MmAssignEntrySla(CacheInfo->ModBitmap, Start, Entry);
		if (Result == 0)
		{
			// Out of memory.
			DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (one block range)");
			KeReleaseMutex(&CacheInfo->ModBitmapMutex);
			return false;
		}

		KeReleaseMutex(&CacheInfo->ModBitmapMutex);
		return true;
	}

	// Start and End are not the same.
	Mask = (MMSLA_ENTRY)(~0ULL << StartInEntry);

	Entry = MmLookUpEntrySla(CacheInfo->ModBitmap, Start);
	Entry |= Mask;

	Result = MmAssignEntrySla(CacheInfo->ModBitmap, Start, Entry);
	if (Result == 0)
	{
		// Out of memory.
		DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (longer range, setting start)");
		KeReleaseMutex(&CacheInfo->ModBitmapMutex);
		return false;
	}

	for (uint64_t i = Start + 1; i < End; i++)
	{
		Result = MmAssignEntrySla(CacheInfo->ModBitmap, i, (MMSLA_ENTRY)(~0ULL));
		if (Result == 0)
		{
			DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (longer range, setting middle)");
			KeReleaseMutex(&CacheInfo->ModBitmapMutex);
			return false;
		}
	}

	Mask = EndInEntry == 63 ? (MMSLA_ENTRY)(~0ULL) : ((MMSLA_ENTRY)(1ULL << (EndInEntry + 1)) - 1);

	Entry = MmLookUpEntrySla(CacheInfo->ModBitmap, End);
	Entry |= Mask;

	Result = MmAssignEntrySla(CacheInfo->ModBitmap, End, Entry);
	if (Result == 0)
	{
		// Out of memory.
		DbgPrint("IoSetRangeModifiedCacheInfo: Out of memory. (longer range, setting end)");
		KeReleaseMutex(&CacheInfo->ModBitmapMutex);
		return false;
	}

	KeReleaseMutex(&CacheInfo->ModBitmapMutex);
	return true;
}
