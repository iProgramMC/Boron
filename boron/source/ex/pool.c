/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ex/pool.c
	
Abstract:
	This module contains the implementation of the
	executive's pool allocator.
	
Author:
	iProgramInCpp - 27 September 2023
***/
#include <ex.h>
#include "../mm/mi.h"

void* ExGetAddressFromHandle(EXMEMORY_HANDLE Handle)
{
	return MiGetAddressFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE)Handle);
}

size_t ExGetSizeFromHandle(EXMEMORY_HANDLE Handle)
{
	return MiGetSizeFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE)Handle);
}

EXMEMORY_HANDLE ExAllocatePool(int PoolFlags, size_t SizeInPages, void** OutputAddress, int Tag)
{
	void* OutputAddressStorage = NULL;
	if (!OutputAddress)
		OutputAddress = &OutputAddressStorage;
	
	EXMEMORY_HANDLE OutHandle = (EXMEMORY_HANDLE) MiReservePoolSpaceTagged (SizeInPages, OutputAddress, Tag, PoolFlags);
	
	if (!OutHandle)
		return OutHandle;
	
	if (~PoolFlags & POOL_FLAG_USER_CONTROLLED)
	{
		bool NonPaged = false;
		if (PoolFlags & POOL_FLAG_NON_PAGED)
			NonPaged = true;
		
		KIPL Ipl = MmLockKernelSpace();
		
		// Map the memory in!  Ideally this will affect ALL page maps
		if (!MmMapAnonPages(MmGetCurrentPageMap(), (uintptr_t) *OutputAddress, SizeInPages, MM_PTE_READWRITE | MM_PTE_SUPERVISOR | MM_PTE_GLOBAL, NonPaged))
		{
			MmUnlockKernelSpace(Ipl);
			MiFreePoolSpace((MIPOOL_SPACE_HANDLE) OutHandle);
			return (EXMEMORY_HANDLE) 0;
		}
		
		MmUnlockKernelSpace(Ipl);
	}
	
	return OutHandle;
}

void ExFreePool(EXMEMORY_HANDLE Handle)
{
	int PoolFlags = (int) MiGetUserDataFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE) Handle);
	
	if (~PoolFlags & POOL_FLAG_USER_CONTROLLED)
	{
		KIPL Ipl = MmLockKernelSpace();
		
		// De-allocate the memory first.  Ideally this will affect ALL page maps
		MmUnmapPages(MmGetCurrentPageMap(),
					 (uintptr_t)ExGetAddressFromHandle(Handle),
					 ExGetSizeFromHandle(Handle));
		
		MmUnlockKernelSpace(Ipl);
	}
	
	// Then release its pool space handle
	MiFreePoolSpace((MIPOOL_SPACE_HANDLE) Handle);
}


void* ExAllocateSmall(size_t Size)
{
	return MiSlabAllocate(Size);
}

void ExFreeSmall(void* Pointer, size_t Size)
{
	return MiSlabFree(Pointer, Size);
}
