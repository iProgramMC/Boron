/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pool.h
	
Abstract:
	This header file defines the pool allocator's API.
	
Author:
	iProgramInCpp - 27 November 2023
***/
#pragma once

typedef int POOL_TYPE;

// If the allocation is not paged.
#define POOL_FLAG_NON_PAGED (1 << 0)

#define POOL_NONPAGED POOL_FLAG_NON_PAGED

// If the pool allocation should only return the range itself,
// but the range itself is unmapped
#define POOL_FLAG_CALLER_CONTROLLED (1 << 1)

// If this flag is set, then the PTEs will be unmapped automatically
// even if POOL_FLAG_CALLER_CONTROLLED is set.
#define POOL_FLAG_UNMAP_ANYWAY (1 << 2)

// Redundant, could just pass 0
#define POOL_PAGED (0)

#define POOL_TAG(x) *((int*)x)

// ******* Big Pool *******
// The big pool is a pool allocation system which returns page
// aligned addresses within the pool area of memory.

void* MmAllocatePoolBig(int PoolFlags, size_t PageCount, int Tag);

// NOTE: This accepts any offset within the page. So even MmMapIoSpace mapped items can be freed with this.
void MmFreePoolBig(void* Address);

size_t MmGetSizeFromPoolAddress(void* Address);

// Maps physical memory with certain caching attributes into pool space.  To free this I/O space,
// simply call MmFreePoolBig.  This function is thread-safe.
//
// The PermissionsAndCaching parameter is ORed onto the PTEs that will map this physical area.
void* MmMapIoSpace(uintptr_t PhysicalAddress, size_t SizeBytes, uintptr_t PermissionsAndCaching, int Tag);

// ******* Little Pool *******
// The little pool is a pool allocation system implemented on top
// of the big pool. It is backed by a slab allocator.
// The little pool allocator is *guaranteed* to return QWORD aligned
// addresses, but not page aligned addresses. For that, use the big pool.

// Allocate pool memory.
// * The POOL_FLAG_CALLER_CONTROLLED flag is not supported and will return NULL.
void* MmAllocatePool(int PoolFlags, size_t Size);

void MmFreePool(void* Pointer);

// Shorthand function to allocate a thread's kernel stack using default parameters.
FORCE_INLINE
void* MmAllocateKernelStack()
{
	return MmAllocatePoolBig(POOL_FLAG_NON_PAGED, KERNEL_STACK_SIZE / PAGE_SIZE, POOL_TAG("ThSt"));
}

#define MmFreeThreadStack(p) MmFreePoolBig(p)
