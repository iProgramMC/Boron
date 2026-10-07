/***
	The Boron Operating System
	Copyright (C) 2025-2026 iProgramInCpp

Module name:
	mm/view.h
	
Abstract:
	This header defines the memory manager's View object as well as its
	interface functions.
	
Author:
	iProgramInCpp - 3 October 2026
***/
#pragma once

#include <mm/pfn.h>

typedef union
{
	// N.B.  Pfn == 0 means the entry is not present and should be (re)filled in
	// from the backing store.
	struct
	{
	#ifdef IS_32_BIT
		MMPFN Pfn : 27;
	#else
		MMPFN Pfn : 32;
		uintptr_t Spare : 27;
	#endif
	
		// The permissions associated with this page.
		unsigned Permissions : 3;
		// Copy-on-write: If reading, use the PFN stored inside. However,
		// if writing, copy the contents of the PFN into a new PFN and
		// clear this bit.
		unsigned CopyOnWrite : 1;
		// If this bit is zero, then an access on this page WILL NOT work
		// and will instead throw an access violation.
		unsigned Committed : 1;
	};
	
	uintptr_t LongEntry;
}
MMVIEW_ENTRY, *PMMVIEW_ENTRY;

static_assert(sizeof(MMVIEW_ENTRY) == sizeof(uintptr_t), "MMVIEW_ENTRY has to be pointer sized.");

typedef union
{
	struct
	{
		// Pages are copy-on-write by default.
		unsigned Private : 1;
		
		// Pages are committed by default.
		unsigned Committed : 1;
		
		// Default permissions
		unsigned Permissions : 3;
	};

	uintptr_t LongFlags;
}
MMVIEW_FLAGS, *PMMVIEW_FLAGS;

typedef struct
{
	PMMVIEW_ENTRY Entries;
	
	size_t SizePages;
	
	MMVIEW_FLAGS Flags;
	
	// Offset into the backing object.
	uint64_t SectionOffset;
	
	// A mappable object, such as a section or file, is referenced here.
	// If NULL, this view is backed by physical memory (and, eventually, page files) instead.
	void* BackingObject;
	
#ifdef IS_32_BIT
	// TODO: Remove this on 32-bit.
	void* TemporaryPageSizeBuffer;
#endif
}
MMVIEW, *PMMVIEW;

// **NOTE**: These functions are not thread safe.  The VAD list lock must be held,
// or you must be the only thread that owns a reference to the view object.

// Creates a view, optionally backed by a mappable object, which can be mapped into
// a user process' address space.
BSTATUS MmCreateView(
	void* BackingObject,
	uint64_t SectionOffset,
	size_t SizePages,
	bool Private,
	bool Commit,
	int CommitPermissions,
	PMMVIEW* OutView
);

// Commits one or more pages inside of a view.
BSTATUS MmCommitView(PMMVIEW View, uintptr_t Offset, size_t SizePages, int Permissions);

// Decommits one or more pages inside of a view.
void MmDecommitView(PMMVIEW View, uintptr_t Offset, size_t SizePages);

// Creates a new view from the old view.
BSTATUS MmCloneView(PMMVIEW InView, PMMVIEW* OutView);

// Marks a view as copy-on-write if needed.
// This function cannot fail.
void MmCopyOnWriteView(PMMVIEW View);

// Acquires information about a memory region inside of a view.
// A memory region is defined here as a range of memory where all parameters (Permissions +
// Committed state) are identical.
//
// NOTE: EntryFlags does NOT contain a valid PFN reference and the Pfn field should be
// ignored.
//
// NOTE: Entry.CopyOnWrite means the entire mapping is private, NOT that each individual
// page is privately mapped.  This is because some pages may still be in their copy-on-write
// state and reference the backing object rather than a private copy. 
BSTATUS MmQueryView(
	PMMVIEW View,
	uintptr_t Offset,
	PMMVIEW_ENTRY OutEntryFlags,
	uintptr_t* OutBaseOffset,
	size_t* OutRegionSizePages
);

// Get a reference to the backing object, if it exists, or NULL for anonymous memory.
void* MmGetBackingObjectView(PMMVIEW View);

// Get the section offset for a view.
uint64_t MmGetSectionOffsetView(PMMVIEW View);
