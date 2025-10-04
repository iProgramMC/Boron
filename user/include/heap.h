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
#include <csect.h>

typedef struct
{
	// The block list is used for fast coalescing.  Every entry *should*
	// be in order of address, and this is asserted by OSDebugDumpHeap.
	LIST_ENTRY BlockList;

	// The free list is used to determine where to allocate.
	LIST_ENTRY FreeList;
	
	// The critical section that guards the whole structure.
	OS_CRITICAL_SECTION CriticalSection;
}
OS_HEAP, *POS_HEAP;

#ifdef __cplusplus
extern "C" {
#endif

// Allocates a block on the heap.  Returns NULL if a block couldn't be allocated.
void* OSAllocateHeap(POS_HEAP Heap, size_t Size);

// Frees a block returned by OSAllocateHeap.  You must only pass NULL or a value
// returned by OSAllocateHeap, otherwise behavior is undefined.
void OSFreeHeap(POS_HEAP Heap, void* Memory);

// Initialize a heap instance.
BSTATUS OSInitializeHeap(POS_HEAP Heap);

// Delete a heap instance.  Currently this must be used only after everything has been freed.
void OSDeleteHeap(POS_HEAP Heap);

#ifdef IS_BORON_DLL
// Initializes the global heap.
void OSDLLInitializeGlobalHeap(void);
#endif

// Allocates a block on the global heap.
void* OSAllocate(size_t Size);

// Frees a block on the global heap.
void OSFree(void* Memory);

#ifdef __cplusplus
}
#endif
