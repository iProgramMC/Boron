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

// Redundant, could just pass 0
#define POOL_PAGED (0)

typedef uintptr_t BIG_MEMORY_HANDLE;

#define POOL_TAG(x) *((int*)x)

#define POOL_NO_MEMORY_HANDLE ((BIG_MEMORY_HANDLE) 0)

// ******* Big Pool *******
// The big pool is a pool allocation system which returns page
// aligned addresses within the pool area of memory.

BIG_MEMORY_HANDLE MmAllocatePoolBig(int PoolFlags, size_t PageCount, void** OutputAddress, int Tag);

void MmFreePoolBig(BIG_MEMORY_HANDLE Handle);

void* MmGetAddressFromBigHandle(BIG_MEMORY_HANDLE Handle);

size_t MmGetSizeFromBigHandle(BIG_MEMORY_HANDLE Handle);

// ******* Little Pool *******
// The little pool is a pool allocation system implemented on top
// of the big pool. It is backed by a slab allocator.
// The little pool allocator is *guaranteed* to return QWORD aligned
// addresses, but not page aligned addresses. For that, use the big pool.

// Allocate pool memory.
// * The POOL_FLAG_CALLER_CONTROLLED flag is not supported and will return NULL.
void* MmAllocatePool(int PoolFlags, size_t Size);

void MmFreePool(void* Pointer);
