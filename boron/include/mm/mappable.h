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
// Returns STATUS_MORE_PROCESSING_REQUIRED if the page needs to be read from disk,
// STATUS_OUT_OF_FILE_BOUNDS if the section's maximum size was reached.
//
// Note: SectionOffset is given in *pages*, not in *bytes*.
typedef BSTATUS(*MM_MAPPABLE_GET_PAGE_FUNC)(
	void* MappableObject,
	uint64_t SectionOffset,
	PMMPFN OutPfn
);

// Reads a page from a file in memory.  This is used solely for files.
typedef BSTATUS(*MM_MAPPABLE_READ_PAGE_FUNC)(
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
	
	MM_MAPPABLE_READ_PAGE_FUNC ReadPage;
	
	MM_MAPPABLE_PREPARE_WRITE_FUNC PrepareWrite;
}
MAPPABLE_DISPATCH_TABLE, *PMAPPABLE_DISPATCH_TABLE;

typedef struct
{
	PMAPPABLE_DISPATCH_TABLE Dispatch;
}
MAPPABLE_HEADER, *PMAPPABLE_HEADER;

#ifdef KERNEL

FORCE_INLINE
BSTATUS MmGetPageMappable(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PMAPPABLE_HEADER Header = MappableObject;
	return Header->Dispatch->GetPage(MappableObject, SectionOffset, OutPfn);
}

FORCE_INLINE
BSTATUS MmReadPageMappable(void* MappableObject, uint64_t SectionOffset, PMMPFN OutPfn)
{
	PMAPPABLE_HEADER Header = MappableObject;
	
	if (Header->Dispatch->ReadPage)
	{
		return Header->Dispatch->ReadPage(MappableObject, SectionOffset, OutPfn);
	}
	
	return STATUS_UNSUPPORTED_FUNCTION;
}

FORCE_INLINE
BSTATUS MmPrepareWriteMappable(void* MappableObject, uint64_t SectionOffset)
{
	PMAPPABLE_HEADER Header = MappableObject;
	return Header->Dispatch->PrepareWrite(MappableObject, SectionOffset);
}

#endif
