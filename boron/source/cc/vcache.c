/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc/vcache.c
	
Abstract:
	This module implements the view cache object for the Boron
	memory manager.
	
Author:
	iProgramInCpp - 4 June 2025
***/
#include "cci.h"
#include <io.h>

// Purge any VADs associated with this FCB.
void CcPurgeViewsForFile(PFCB Fcb)
{
	// NOTE: This is *only* meant to be called when dereferencing
	// the FCB for the last time, and it's about to be wiped out!
	//
	// However we will still protect against race conditions just in case.
	KIPL Ipl;
	KeAcquireSpinLock(&Fcb->ViewCacheLock, &Ipl);
	
	while (!IsEmptyRbTree(&Fcb->ViewCache))
	{
		ASSERT(!Fcb->ViewCacheLock.Locked);
		
		// Get the first entry and remove it.
		PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&Fcb->ViewCache);
		RemoveItemRbTree(&Fcb->ViewCache, Entry);
		KeReleaseSpinLock(&Fcb->ViewCacheLock, Ipl);
		
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, ViewCacheEntry);
		MmUnmapViewOfFileInSystemSpace((void*) Vad->Node.StartVa, false);
		
		KeAcquireSpinLock(&Fcb->ViewCacheLock, &Ipl);
	}
	
	ASSERT(IsEmptyRbTree(&Fcb->ViewCache));
	KeReleaseSpinLock(&Fcb->ViewCacheLock, Ipl);
}

/*
// Reads the contents of a file and copies them to a buffer.
BSTATUS CcReadFileCopy(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	void* Buffer,
	size_t Size,
	bool MayBlock,
	uint32_t Flags
)
{
}
*/
