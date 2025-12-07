/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/mappable.h
	
Abstract:
	This header defines the memory manager's mappable
	object interface.

Author:
	iProgramInCpp - 7 December 2025
***/
#pragma once

// Gets a page at an offset.  This is a read operation, in that the mappable object
// is not being modified.  The page frame number returned will have a new reference
// to it which must be freed if the page turns out not to be used.
//
// Returns PFN_INVALID if the page needs to be read from disk.
//
// Note: SectionOffset is given in *pages*, not in *bytes*.
typedef BSTATUS(*MM_MAPPABLE_GET_PAGE_FUNC)(
	void* MappableObject,
	uint64_t SectionOffset,
	PMMPFN OutPfn
);

// Prepares a page for writing.  In the case of CoW overlays, it duplicates the page
// from this object's parent.
typedef BSTATUS(*MM_MAPPABLE_PREPARE_WRITE_FUNC)(
	void* MappableObject,
	uint64_t SectionOffset
);

typedef struct
{
	MM_MAPPABLE_GET_PAGE_FUNC GetPage;
	
	MM_MAPPABLE_PREPARE_WRITE_FUNC PrepareWrite;
}
MAPPABLE_DISPATCH_TABLE, *PMAPPABLE_DISPATCH_TABLE;

typedef struct
{
	PMAPPABLE_DISPATCH_TABLE Dispatch;
}
MAPPABLE_HEADER, *PMAPPABLE_HEADER;
