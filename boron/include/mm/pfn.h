/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pfn.h
	
Abstract:
	This header file contains the definitions for the
	page frame number (PFN) database.
	
Author:
	iProgramInCpp - 28 September 2023
***/
#ifndef BORON_MM_PFN_H
#define BORON_MM_PFN_H

#include <main.h>

// Page frame number.
//
// TODO: An 'int' PFN is sufficient for now. It allows up to 16 TB
// of physical memory to be represented right now, which is 1024 times
// what's in my computer.  But if needed, you should change this to
// a uintptr_t.
typedef int MMPFN, *PMMPFN;

// Page frame database entry structure.
//
// NOTE: Fields prefixed with underscore are not meant to be accessed
// directly and should instead be accessed via a macro!
typedef struct
{
	// Flags for the specifically referenced page frame.
	struct
	{
		unsigned Type : 4;
		
		// Flags
		
		// If this flag is set, then, when every reference is removed from
		// the page frame, and it's part of a CCB, then this page is placed
		// on the modified page list and will be written to the referenced
		// file.
		unsigned Modified : 1;
		
		// If this flag is set, then this page is part of the Modified page list.
		unsigned IsInModifiedPageList : 1;
		
		// If this flag is set, then this page is part of a file's page cache.
		unsigned IsFileCache : 1;
		
		unsigned Spare : 9;
		
		// The upper part of the offset within the page cache of a file, if needed.
		unsigned _OffsetUpper : 16;
	};
	
	// Disregard if this is allocated.  Eventually these will be part of a union
	// where they will take on different roles depending on the role of the page.
	MMPFN NextFrame;
	MMPFN PrevFrame;
	
	// Disregard if this is free. Eventually this will be part of a union
	// where it will take on different roles depending on the role of the
	// page.
	int RefCount;
	
#ifdef IS_64_BIT
	union
	{
		struct
		{
			// Prototype PTE.  This points to the entry in an FCB's page cache, if this
			// page is found on the standby list.
			uint64_t _PrototypePte : 48;
			
			// The pointer to the FCB that contains this file page.
			uint64_t _Fcb : 48;
			
			// The lower 32-bit offset of this page.  Note that this offset is counted
			// in multiples of 4KB.
			uint64_t _OffsetLower : 32;
		}
		PACKED
		FileCache;
	};
#else
	union
	{
		struct
		{
			// This address is a **physical** address.  Therefore, 
			uint32_t _PrototypePte;
			
			uint32_t _Fcb;
			
			uint32_t _OffsetLower;
		}
		PACKED
		FileCache;
	};
	
	uint32_t Dummy; // to make this a power of 2
#endif
}
PACKED
MMPFDBE, *PMMPFDBE;

#ifdef IS_64_BIT

#define PFDBE_PrototypePte(Pfdbe) ((MMPFN*) (0xFFFF000000000000ULL | (Pfdbe)->FileCache._PrototypePte))
#define PFDBE_Fcb(Pfdbe)          ((PFCB)   (0xFFFF000000000000ULL | (Pfdbe)->FileCache._Fcb))

#else

#define PFDBE_PrototypePte(Pfdbe) ((MMPFN*) ((Pfdbe)->FileCache._PrototypePte))
#define PFDBE_Fcb(Pfdbe)          ((PFCB)   ((Pfdbe)->FileCache._Fcb))

#endif

#define PFDBE_Offset(Pfdbe)       ((uint64_t)(Pfdbe)->FileCache._OffsetLower | ((uint64_t)(Pfdbe)->_OffsetUpper << 16))

#define PFN_FLAG_CAPTURED (1 << 0)

enum
{
	PF_TYPE_FREE,
	PF_TYPE_ZEROED,
	PF_TYPE_USED,
	PF_TYPE_RECLAIM,
	PF_TYPE_TRANSITION,
	PF_TYPE_MMIO,
};

#define PFN_INVALID ((MMPFN)-1)

// Returned by the page cache.  Watch out!
#define MM_PFN_OUTOFMEMORY ((MMPFN) -2)

#define IS_BAD_PFN(Pfn) ((Pfn) == PFN_INVALID || (Pfn) == MM_PFN_OUTOFMEMORY)

#ifdef IS_64_BIT
static_assert((sizeof(MMPFDBE) & (sizeof(MMPFDBE) - 1)) == 0,  "The page frame struct should be a power of two");
#endif

#endif//BORON_MM_PFN_H
