/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ex.h
	
Abstract:
	This module contains the implementation of the
	executive's pool allocator.
	
Author:
	iProgramInCpp - 27 September 2023
***/
#ifndef BORON_EX_H
#define BORON_EX_H

#include <main.h>

#include <ex/aatree.h>

// Four character tag.
#define EX_TAG(TagStr) (*((int*)TagStr))

#define EX_NO_MEMORY_HANDLE ((EXMEMORY_HANDLE) 0)

// ====== Big allocations ======
// If the allocation is instant, i.e. you don't want to take page faults on the memory.
// This is important for some uses.
#define POOL_FLAG_NON_PAGED (1 << 0)

#define POOL_FLAG_USER_CONTROLLED (1 << 1)

// Handle to a memory location. This does not represent the actual address.
typedef uintptr_t EXMEMORY_HANDLE;

// Obtain the address from a memory handle.
void* ExGetAddressFromHandle(EXMEMORY_HANDLE);

// Obtain the allocated size (in pages) from a memory handle.
size_t ExGetSizeFromHandle(EXMEMORY_HANDLE Handle);

// Reserve space in the kernel pool. Returns NULL if allocation failed.
// Don't depend on OutputAddress not changing if allocation fails.
EXMEMORY_HANDLE ExAllocatePool(int PoolFlags, size_t SizeInPages, void** OutputAddress, int Tag);

// Free space in the kernel pool.
void ExFreePool(EXMEMORY_HANDLE);

// ====== Small allocations ======
void* ExAllocateSmall(size_t Size);

void ExFreeSmall(void* Pointer, size_t Size);

#endif//BORON_EX_H
