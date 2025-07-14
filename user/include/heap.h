/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	heap.h
	
Abstract:
	This header defines heap APIs for Boron userspace applications.
	
Author:
	iProgramInCpp - 14 July 2025
***/
#pragma once

#include <list.h>

typedef struct
{
	// The block list is used for fast coalescing.  Every entry *should*
	// be in order of address, and this is asserted by OSDebugDumpHeap.
	LIST_ENTRY BlockList;

	// The free list is used to determine where to allocate.
	LIST_ENTRY FreeList;
}
OS_HEAP, *POS_HEAP;

// Allocates a block on the heap.  Returns NULL if a block couldn't be allocated.
void* OSAllocateHeap(POS_HEAP Heap, size_t Size);

// Frees a block returned by OSAllocateHeap.  You must only pass NULL or a value
// returned by OSAllocateHeap, otherwise behavior is undefined.
void OSFreeHeap(POS_HEAP Heap, void* Memory);

// Initialize a heap instance.
void OSInitializeHeap(POS_HEAP Heap);
