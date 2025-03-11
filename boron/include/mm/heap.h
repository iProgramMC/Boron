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
	size_t ItemSize;
}
MMHEAP, *PMMHEAP;

// Initializes a heap.
//
// The heap manages items of size ItemSize, done this way to allow MMVADs
// to be administered and dished out without reallocations.
//
// It initializes the heap with one free segment which starts at
// InitialVa (which is a VPN), and spans InitialSize pages.
void MmInitializeHeap(PMMHEAP Heap, size_t ItemSize, uintptr_t InitialVa, size_t InitialSize);

// Allocates a segment of address space from the heap.  This returns its MMADDRESS_NODE.
//
// Ways this can fail:
// - If the entry needed to be split up and another MMADDRESS_NODE couldn't be allocated.
//   (STATUS_INSUFFICIENT_MEMORY)
// - If there are no segments big enough to fit. (STATUS_INSUFFICIENT_VA_SPACE)
//
// Returns the MMADDRESS_NODE associated with that address, or the reason why virtual address
// allocation failed.  It should be reused for the VAD list.
BSTATUS MmAllocateAddressSpace(PMMHEAP Heap, size_t SizePages, bool TopDown, PMMADDRESS_NODE* OutNode);

// Returns a segment of memory back to the heap, marking it free.  Does not unmap the pages
// or anything like that.
//
// The segment passed in MUST have been REMOVED from the VAD list.
BSTATUS MmFreeAddressSpace(PMMHEAP Heap, PMMADDRESS_NODE Node);

