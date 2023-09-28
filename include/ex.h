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

// If the allocation is instant, i.e. you don't want to take page faults on the memory.
// This is important for some uses.
#define POOL_FLAG_INSTANT (1 << 0)

// Handle to a memory location. This does not represent the actual address.
typedef uintptr_t EXMEMORY_HANDLE;

// Obtain the address from a memory handle.
void* ExGetAddressFromHandle(EXMEMORY_HANDLE);

// Reserve space in the kernel pool.
EXMEMORY_HANDLE ExAllocatePool(int PoolFlags, size_t SizeInPages, void** OutputAddress, int Tag);

// Free space in the kernel pool.
void ExFreePool(EXMEMORY_HANDLE);

#endif//BORON_EX_H
