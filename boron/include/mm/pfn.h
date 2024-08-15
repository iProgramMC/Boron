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
// Keep this structure's size a power of 2.
typedef struct
{
	// Flags for the specifically referenced page frame.
	int Type : 4;
	int Flags : 28;
	
	// Disregard if this is allocated.  Eventually these will be part of a union
	// where they will take on different roles depending on the role of the page.
	MMPFN NextFrame;
	MMPFN PrevFrame;
	
	// Disregard if this is free. Eventually this will be part of a union
	// where it will take on different roles depending on the role of the
	// page.
	int RefCount;
	
	// Prototype PTE.  This points to the entry in an FCB's page cache, if this
	// page is found on the standby list.
	// TODO: Use this
	//uint64_t PrototypePte;
	
	// Unused for now.
	//uint64_t qword3;
}
MMPFDBE, *PMMPFDBE;

#define PFN_FLAG_CAPTURED (1 << 0)

enum
{
	PF_TYPE_FREE,
	PF_TYPE_ZEROED,
	PF_TYPE_USED,
	PF_TYPE_RECLAIM,
};

#define PFN_INVALID ((MMPFN)-1)

static_assert((sizeof(MMPFDBE) & (sizeof(MMPFDBE) - 1)) == 0,  "The page frame struct should be a power of two");

#endif//BORON_MM_PFN_H
