/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/addrnode.h
	
Abstract:
	This header defines the heap manager specific functions.
	
	The heap manager does the job of managing FREE address regions for
	user space.  For the USED address region manager, see mm/vad.h.
	
Author:
	iProgramInCpp - 22 September 2024
***/
#pragma once

#include <mm/addrnode.h>
#include <ke.h>

// The MMHEAP struct describes a tree of FREE ranges.  The VAD list describes a list of USED ranges.
typedef struct
{
	RBTREE Tree;
	KMUTEX Mutex;
}
MMHEAP, *PMMHEAP;

// Initializes a heap.
void MmInitializeHeap(PMMHEAP Heap);

// Allocates a segment of address space from the heap.  This returns its MMADDRESS_NODE.
//
// Ways this can fail:
// - If the entry needed to be split up and another MMADDRESS_NODE couldn't be allocated.
// - If there are no segments big enough to fit.
//
// Returns the MMADDRESS_NODE associated with that address.  It should be reused for the
// VAD list.
PMMADDRESS_NODE MmAllocateAddressSpace(PMMHEAP Heap, size_t SizePages);

// Returns a segment of memory back to the heap, marking it free.  Does not unmap the pages
// or anything like that.
//
// The segment passed in MUST have been REMOVED from the VAD list.
bool MmFreeAddressSpace(PMMHEAP Heap, PMMADDRESS_NODE Node);

