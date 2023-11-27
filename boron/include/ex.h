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

// THESE ARE DEPRECATED, and will be removed in a future version.
#include <mm.h>
#define EX_TAG POOL_TAG
#define EX_NO_MEMORY_HANDLE POOL_NO_MEMORY_HANDLE
#define EXMEMORY_HANDLE BIG_MEMORY_HANDLE
#define POOL_FLAG_USER_CONTROLLED POOL_FLAG_CALLER_CONTROLLED
#define ExAllocatePool MmAllocatePoolBig
#define ExFreePool MmFreePoolBig
#define ExGetAddressFromHandle MmGetAddressFromBigHandle
#define ExGetSizeFromHandle MmGetSizeFromBigHandle

void* ExAllocateSmall(size_t Size);
void ExFreeSmall(void* Pointer, size_t Size);

#endif//BORON_EX_H
