/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/view.h
	
Abstract:
	This header defines the memory manager's View object and
	interfaces related to it.
	
Author:
	iProgramInCpp - 2 October 2026
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

// Creates a view, optionally backed by a mappable object, which can be mapped into
// a user process' address space.
BSTATUS MmCreateView(
	void* BackingObject,
	uint64_t SectionOffset,
	size_t SizePages,
	bool Private,
	bool Commit,
	PMMVIEW* OutView
);

// Commits one or more pages inside of a view.
BSTATUS MmCommitView(PMMVIEW View, uintptr_t Offset, size_t SizePages);

// Decommits one or more pages inside of a view.
BSTATUS MmDecommitView(PMMVIEW View, uintptr_t Offset, size_t SizePages);

// Creates a new view from the old view.
BSTATUS MmCloneView(PMMVIEW InView, PMMVIEW* OutView);

// Deletes a view.
void MmDeleteView(PMMVIEW View);
