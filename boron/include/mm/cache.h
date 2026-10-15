/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/cache.h

Abstract:
	This header defines the memory manager's page cache
	control block structure.

Author:
	iProgramInCpp - 18 August 2024
***/
#pragma once

#include <ke.h>
#include "sla.h"

#define MM_CCB_MUTEX_LEVEL    4

typedef struct _FCB FCB, *PFCB;
typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;

typedef struct _CCB
{
	// This mutex is locked when allocating indirection tables.
	//
	// This mutex is NOT locked when setting or clearing an entry
	// (instead, an atomic operation is performed)
	//
	// Note: the PFN lock is held while mutating an entry, to prevent
	// the physical page it describes from being freed during the
	// operation.
	KMUTEX Mutex;
	MMSLA Sla;
}
CCB, *PCCB;

void MmInitializeCcb(PCCB Ccb);

void MmTearDownCcb(PCCB Ccb);

ALWAYS_INLINE static inline
void MmLockCcb(PCCB Ccb)
{
	BSTATUS Status = KeWaitForSingleObject(&Ccb->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(Status == STATUS_SUCCESS);
}

ALWAYS_INLINE static inline
void MmUnlockCcb(PCCB Ccb)
{
	KeReleaseMutex(&Ccb->Mutex);
}

typedef bool(*PCC_FOREACH_FILTER_CALL)(MMPFN Pfn, uint64_t Offset, void* FilterContext);
typedef BSTATUS(*PCC_FOREACH_PROCESS_CALL)(MMPFN Pfn, uint64_t Offset, void* ProcessEntryContext);

#if 0

// Gets a pointer to an entry in the CCB.
// If TryAllocateLowerLevels is true, it will attempt to allocate levels if they aren't
// allocated.  However, this might fail if out of memory, in which case NULL will be
// returned.
//
// NOTE: PageOffset is the page *number* within the file and *NOT* the byte offset.
//
// NOTE: The CCB must be locked.
PCCB_ENTRY MmGetEntryPointerCcb(PCCB Ccb, uint64_t PageOffset, bool TryAllocateLowerLevels);

#endif

// Retrieves the page frame number at the specified page offset within the CCB.
// This returns a PFN whose reference count is incremented by one on retrieval.
//
// This is thread safe because the CCB mutex is used internally.
MMPFN MmGetEntryCcb(PCCB Ccb, uint64_t PageOffset);

// Assigns a PFN to the specified page offset within the CCB.
//
// OutPrototypePtePointer is nullable.
//
// If Pfn is PFN_INVALID, then this serves to:
// 1) prepare the CCB for assignment in this slot (as a performance optimization), and
// 2) check if there is already a PFN assigned to this slot (to refault instead of doing
//    a useless write)
//
// If the entry is already assigned, this returns STATUS_CONFLICTING_ADDRESSES.
BSTATUS MmSetEntryCcb(PCCB Ccb, uint64_t PageOffset, MMPFN Pfn, PMM_PROTOTYPE_PTE_PTR OutPrototypePtePointer);

//
// Optimized routine to scan for each populated entry inside of a page cache.
//
// If an entry is found, call into the Filter function with the CCB lock held
// to evaluate whether the entry should be processed, and if Filter returns true,
// call the ProcessEntry function with the entry unlocked.
//
// If ProcessEntry returns an unsuccessful status, then processing is aborted.
//
BSTATUS MmForEachEntryCcb(
	PCCB Ccb,
	PCC_FOREACH_FILTER_CALL Filter,
	PCC_FOREACH_PROCESS_CALL ProcessEntry,
	void* FilterContext,
	void* ProcessEntryContext
);

// Flushes the page cache of a file object.
BSTATUS MmFlushFileCache(PFILE_OBJECT FileObject);
