/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/sla.h
	
Abstract:
	This header defines the memory manager's sparse
	linear array data structure.
	
Author:
	iProgramInCpp - 7 December 2025
***/
#pragma once

typedef struct _MMSLA MMSLA, *PMMSLA;

typedef uintptr_t MMSLA_ENTRY, *PMMSLA_ENTRY;

//
// Note: these calculations assume 4096 byte pages
//
// 64-bit Entries:
//
// Per level: 512 Entries at the end, 1024 Entries per indirection
// Level 0: 64 (256 KB)
// Level 1: 512 (2 MB)
// Level 2: 524288 (2 GB)
// Level 3: 536870912 (2 TB)
// Level 4: 549755813888 (2 PB)
//
// 32-bit Entries:
//
// Per level: 1024 Entries
// Level 0: 64 (256 KB)
// Level 1: 1024 (4 MB)
// Level 2: 1048576 (4 GB)
// Level 3: 1073741824 (4 TB)
// Level 4: 1099511627776 (4 PB)
//
// As of 07/12/2025, each PFN can be stored in just 32 bits, so theoretically
// we may use 32-bit entries. However, that only applies to the page cache.
// entries. For anonymous sections and CoW overlays, we may need additional info.
//
// However, if this additional info turns out not to be needed, feel free to
// just use 32-bit entries throughout.
//
// (Or if the info is only a couple of bits then you could just restrict the
// physical memory space to 1 TB or something instead of 16 TB with 32-bit PFNs)
//
// NOTE: This structure is not thread safe. Users must guard it with a mutex or
// an rwlock.
//

#define MM_SLA_DIRECT_ENTRY_COUNT (64)
#define MM_SLA_INDIRECT_LEVELS    (4)

#define MM_SLA_NO_DATA       ((MMSLA_ENTRY) 0xFFFFFFFFFFFFFFFF)
#define MM_SLA_OUT_OF_MEMORY ((MMSLA_ENTRY) 0xFFFFFFFFFFFFFFFE)

typedef uintptr_t MMSLA_ENTRY;

typedef struct _MMSLA
{
	MMSLA_ENTRY Direct[MM_SLA_DIRECT_ENTRY_COUNT];
	MMPFN Indirect[MM_SLA_INDIRECT_LEVELS];
}
MMSLA, *PMMSLA;

typedef void(*MM_SLA_REFERENCE_ENTRY)(PMMSLA_ENTRY Entry);

typedef void(*MM_SLA_FREE_ENTRY)(MMSLA_ENTRY Entry);

// Initializes the sparse linear array data structure.
void MmInitializeSla(PMMSLA Sla);

// De-initializes the SLA data structure by freeing every entry
// and removing all indirections from memory.
void MmDeinitializeSla(PMMSLA Sla, MM_SLA_FREE_ENTRY FreeEntry);

// Looks up an entry from the SLA.  Returns MM_SLA_NO_DATA if the
// entry doesn't exist.
//
// If the entry exists, then EntryReferenceFunc will be called to ensure
// that this entry cannot be modified once this function exits. This behavior
// is used by the page cache to prevent the standby page reclamation code from
// reclaiming a page while it is being referenced elsewhere.
//
// Note that EntryReferenceFunc may be called within a MmBeginUsingHHDM()
// context.  This is important to know, because in that state, you may not
// acquire any mutexes, and you are heavily restricted.
MMSLA_ENTRY MmLookUpEntrySlaEx(
	PMMSLA Sla,
	uint64_t EntryIndex,
	MM_SLA_REFERENCE_ENTRY EntryReferenceFunc
);

// Assigns an entry to the SLA.  Returns MM_SLA_OUT_OF_MEMORY if the
// assignment couldn't proceed due to an out of memory error.
//
// OutPrototypePtePointer - The resulting prototype pointer, if requested.
//   This pointer can be used for direct access into the data structure, if
//   such access is required. (For example, the standby page reclamation code
//   will use this pointer to write directly into the page bypassing any mutex
//   guarding the structure.
MMSLA_ENTRY MmAssignEntrySlaEx(
	PMMSLA Sla,
	uint64_t EntryIndex,
	MMSLA_ENTRY NewValue,
	uintptr_t* OutPrototypePtePointer
);


// Looks up an entry from the SLA.  Returns MM_SLA_NO_DATA if the
// entry doesn't exist.
FORCE_INLINE
MMSLA_ENTRY MmLookUpEntrySla(PMMSLA Sla, uint64_t EntryIndex)
{
	return MmLookUpEntrySlaEx(Sla, EntryIndex, NULL);
}

// Assigns an entry to the SLA.  Returns MM_SLA_OUT_OF_MEMORY if the
// assignment couldn't proceed due to an out of memory error.
FORCE_INLINE
MMSLA_ENTRY MmAssignEntrySla(
	PMMSLA Sla,
	uint64_t EntryIndex,
	MMSLA_ENTRY NewValue
)
{
	return MmAssignEntrySlaEx(Sla, EntryIndex, NewValue, NULL);
}