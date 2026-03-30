/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc/vclru.c
	
Abstract:
	This module implements the view cache LRU list.
	
Author:
	iProgramInCpp - 4 June 2025
***/
#include "cci.h"

static LIST_ENTRY CcViewCacheLru = { .Flink = &CcViewCacheLru, .Blink = &CcViewCacheLru };
static int        CcViewCacheLruSize;
static KSPIN_LOCK CcViewCacheLruLock;

// Adds a VAD to the view cache LRU list.
void CcAddVadToViewCacheLru(PMMVAD Vad)
{
	KIPL Ipl;
	KeAcquireSpinLock(&CcViewCacheLruLock, &Ipl);
	
	if (Vad->ViewCacheLruEntry.Flink == NULL)
	{
		InsertTailList(&CcViewCacheLru, &Vad->ViewCacheLruEntry);
		AtFetchAdd(CcViewCacheLruSize, +1);
	}
	
	KeReleaseSpinLock(&CcViewCacheLruLock, Ipl);
}

// Removes a VAD from the view cache LRU list.
void CcRemoveVadFromViewCacheLru(PMMVAD Vad)
{
	KIPL Ipl;
	KeAcquireSpinLock(&CcViewCacheLruLock, &Ipl);
	
	if (Vad->ViewCacheLruEntry.Flink != NULL)
	{
		RemoveEntryList(&Vad->ViewCacheLruEntry);
		AtFetchAdd(CcViewCacheLruSize, -1);
		
		Vad->ViewCacheLruEntry.Flink = NULL;
		Vad->ViewCacheLruEntry.Blink = NULL;
	}
	
	KeReleaseSpinLock(&CcViewCacheLruLock, Ipl);
}

// Moves a VAD to the front of the view cache LRU list.
void CcOnSystemSpaceVadUsed(PMMVAD Vad)
{
	KIPL Ipl;
	KeAcquireSpinLock(&CcViewCacheLruLock, &Ipl);
	
	if (Vad->ViewCacheLruEntry.Flink != NULL)
		RemoveEntryList(&Vad->ViewCacheLruEntry);
	
	InsertTailList(&CcViewCacheLru, &Vad->ViewCacheLruEntry);
	
	KeReleaseSpinLock(&CcViewCacheLruLock, Ipl);
}

// Gets the head of the view cache LRU list for freeing.
// This is used if view space is running out.
PMMVAD CcGetHeadOfViewCacheLru()
{
	KIPL Ipl;
	KeAcquireSpinLock(&CcViewCacheLruLock, &Ipl);
	
	if (IsListEmpty(&CcViewCacheLru))
	{
		KeReleaseSpinLock(&CcViewCacheLruLock, Ipl);
		return NULL;
	}
	
	PLIST_ENTRY Entry = CcViewCacheLru.Flink;
	//RemoveHeadList(&CcViewCacheLru);
	//AtFetchAdd(CcViewCacheLruSize, -1);
	KeReleaseSpinLock(&CcViewCacheLruLock, Ipl);
	
	return CONTAINING_RECORD(Entry, MMVAD, ViewCacheLruEntry);
}

// Purge any VADs that go over the limit plus a specified number.
// This is a soft-limit, so it is possible to go over.
void CcPurgeViewsOverLimit(int LeaveSpaceFor)
{
	// NOTE: I realize that there is a possible race condition however
	// it should not allow more than one view to go over the limit.
	int Limit = VIEW_CACHE_MAX_COUNT - LeaveSpaceFor;
	
#ifdef DEBUG
	PMMVAD LastVad = NULL;
#endif
	
	while (AtLoad(CcViewCacheLruSize) > Limit)
	{
		PMMVAD Vad = CcGetHeadOfViewCacheLru();
		ASSERTN(!LastVad || LastVad != Vad);
		
		// This also removes the VAD from the view cache LRU.
		MmUnmapViewOfFileInSystemSpace((void*) Vad->Node.StartVa, true);
#ifdef DEBUG
		LastVad = Vad;
#endif
	}
}

void CcUnmapAllViews()
{
	while (AtLoad(CcViewCacheLruSize) != 0)
	{
		PMMVAD Vad = CcGetHeadOfViewCacheLru();
		MmUnmapViewOfFileInSystemSpace((void*) Vad->Node.StartVa, true);
	}
	
	DbgPrint("CcViewCacheLruSize = %d", CcViewCacheLruSize);
}
