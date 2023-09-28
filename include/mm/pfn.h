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

// Page frame item in the page frame database.
// Keep this a power of 2, please.
// Currently this only reserves 0.7% of physical memory.  Not bad
typedef struct
{
	// Flags for the specifically referenced page frame.
	int Type : 4;
	int Flags : 28;
	
	// Disregard if this is allocated.  Eventually these will be part of a union
	// where they will take on different roles depending on the role of the page.
	int NextFrame;
	int PrevFrame;
	
	// Disregard if this is free. Eventually this will be part of a union
	// where it will take on different roles depending on the role of the
	// page.
	int RefCount;
	
	// The PTE this is used in. Used only in non-shared pages.
	uint64_t PteUsingThis;
	
	// Unused for now.
	uint64_t qword3;
}
MMPFN, *PMMPFN;

enum MMPFN_TYPE_tag
{
	PF_TYPE_FREE,
	PF_TYPE_ZEROED,
	PF_TYPE_USED,
};

#define PFN_INVALID (-1)

#define PFNS_PER_PAGE (PAGE_SIZE / sizeof(MMPFN))

static_assert((sizeof(MMPFN) & (sizeof(MMPFN) - 1)) == 0,  "The page frame struct should be a power of two");

#endif//BORON_MM_PFN_H
