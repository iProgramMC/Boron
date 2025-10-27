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

#define MM_CCB_MUTEX_LEVEL    4

#define MM_INDIRECTION_LEVELS 4
#define MM_DIRECT_PAGE_COUNT  (16 - MM_INDIRECTION_LEVELS)

// Levels of indirection and the amount of extra bytes it would help access, in addition to previous levels:
// L0:  48 KiB (if MM_INDIRECTION_LEVELS==4), 44 KiB (if MM_INDIRECTION_LEVELS == 5)
// L1:   2 MiB
// L2:   1 GiB
// L3: 512 GiB
// L4: 256 TiB
// L5: 128 PiB

// I will go for 4 levels of indirection for now.

typedef struct _FCB FCB, *PFCB;

#define MM_INDIRECTION_COUNT (PAGE_SIZE / sizeof(MMPFN))

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
	
	uint64_t FirstModifiedPage;
	uint64_t LastModifiedPage;
	
	MMPFN Direct[MM_DIRECT_PAGE_COUNT];
	MMPFN Level1Indirect;
	MMPFN Level2Indirect;
	MMPFN Level3Indirect;
	MMPFN Level4Indirect;
#if MM_INDIRECTION_LEVELS == 5
	MMPFN Level5Indirect;
#endif
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
