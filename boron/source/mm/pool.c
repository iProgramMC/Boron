/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pool.c
	
Abstract:
	This module contains the implementation of the pool
	memory allocator API.
	
Author:
	iProgramInCpp - 27 November 2023
***/
#include "mi.h"

void* MmGetAddressFromBigHandle(BIG_MEMORY_HANDLE Handle)
{
	return MiGetAddressFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE)Handle);
}

size_t MmGetSizeFromBigHandle(BIG_MEMORY_HANDLE Handle)
{
	return MiGetSizeFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE)Handle);
}

BIG_MEMORY_HANDLE MmAllocatePoolBig(int PoolFlags, size_t PageCount, void** OutputAddress, int Tag)
{
	void* OutputAddressStorage = NULL;
	if (!OutputAddress)
		OutputAddress = &OutputAddressStorage;
	
	BIG_MEMORY_HANDLE OutHandle = (BIG_MEMORY_HANDLE) MiReservePoolSpaceTagged (
		PageCount,
		OutputAddress,
		Tag,
		PoolFlags);
	
	if (!OutHandle)
		return OutHandle;
	
	if (~PoolFlags & POOL_FLAG_USER_CONTROLLED)
	{
		bool NonPaged = false;
		if (PoolFlags & POOL_FLAG_NON_PAGED)
			NonPaged = true;
		
		KIPL Ipl = MmLockKernelSpace();
		
		// Map the memory in!  This will affect ALL page maps
		if (!MmMapAnonPages(
			MmGetCurrentPageMap(),
			(uintptr_t) *OutputAddress,
			PageCount,
			MM_PTE_READWRITE | MM_PTE_SUPERVISOR | MM_PTE_GLOBAL,
			NonPaged))
		{
			MmUnlockKernelSpace(Ipl);
			MiFreePoolSpace((MIPOOL_SPACE_HANDLE) OutHandle);
			return (BIG_MEMORY_HANDLE) 0;
		}
		
		MmUnlockKernelSpace(Ipl);
	}
	
	return OutHandle;
}

void MmFreePoolBig(BIG_MEMORY_HANDLE Handle)
{
	int PoolFlags = (int) MiGetUserDataFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE) Handle);
	
	if (~PoolFlags & POOL_FLAG_USER_CONTROLLED)
	{
		KIPL Ipl = MmLockKernelSpace();
		
		// De-allocate the memory first.  Ideally this will affect ALL page maps
		MmUnmapPages(MmGetCurrentPageMap(),
					 (uintptr_t)ExGetAddressFromHandle(Handle),
					 MmGetSizeFromBigHandle(Handle));
		
		MmUnlockKernelSpace(Ipl);
	}
	
	// Then release its pool space handle
	MiFreePoolSpace((MIPOOL_SPACE_HANDLE) Handle);
}

void* MmAllocatePool(int PoolFlags, size_t Size)
{
	return MiSlabAllocate(PoolFlags & POOL_FLAG_NON_PAGED, Size);
}

void MmFreePool(void* Pointer)
{
	MiSlabFree(Pointer);
}
