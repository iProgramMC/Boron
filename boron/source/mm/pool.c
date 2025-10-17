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

size_t MmGetSizeFromPoolAddress(void* Address)
{
	MIPOOL_SPACE_HANDLE Handle = MiGetPoolSpaceHandleFromAddress(Address);
	return MiGetSizeFromPoolSpaceHandle(Handle);
}

void* MmAllocatePoolBig(int PoolFlags, size_t PageCount, int Tag)
{
	void* OutputAddress = NULL;
	
	MIPOOL_SPACE_HANDLE OutHandle = (MIPOOL_SPACE_HANDLE) MiReservePoolSpaceTagged (
		PageCount,
		&OutputAddress,
		Tag,
		PoolFlags);
	
	if (!OutHandle)
		return NULL;
	
	if (~PoolFlags & POOL_FLAG_CALLER_CONTROLLED)
	{
		bool NonPaged = false;
		if (PoolFlags & POOL_FLAG_NON_PAGED)
			NonPaged = true;
		
		MmLockKernelSpaceExclusive();
		
		// Map the memory in!  This will affect ALL page maps
		if (!MiMapAnonPages(
			MiGetCurrentPageMap(),
			(uintptr_t) OutputAddress,
			PageCount,
			MM_PTE_READWRITE | MM_PTE_GLOBAL,
			NonPaged))
		{
			MmUnlockKernelSpace();
			MiFreePoolSpace(OutHandle);
			return NULL;
		}
		
		MmUnlockKernelSpace();
	}
	
	return OutputAddress;
}

void MmFreePoolBig(void* Address)
{
	MIPOOL_SPACE_HANDLE Handle = MiGetPoolSpaceHandleFromAddress(Address);
	
	int PoolFlags = (int) MiGetUserDataFromPoolSpaceHandle(Handle);
	if (~PoolFlags & POOL_FLAG_CALLER_CONTROLLED)
	{
		MmLockKernelSpaceExclusive();
		
		// De-allocate the memory first.  Ideally this will affect ALL page maps
		MiUnmapPages(MiGetCurrentPageMap(),
					 (uintptr_t)Address,
					 MiGetSizeFromPoolSpaceHandle(Handle));
		
		MmUnlockKernelSpace();
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

void* MmMapIoSpace(uintptr_t PhysicalAddress, size_t Size, uintptr_t PermissionsAndCaching, int Tag)
{
	if (Tag == 0) Tag = POOL_TAG("MMIS");
	
	size_t SizePages = (Size + PAGE_SIZE - 1) / PAGE_SIZE;
	
	// Allocate some pool space.
	void* Space = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, SizePages, Tag);
	uintptr_t VirtualAddress = (uintptr_t) Space;
	
	if (!Space)
		return Space;
	
	MmLockKernelSpaceExclusive();
	
	HPAGEMAP PageMap = MiGetCurrentPageMap();
	
	for (size_t i = 0; i < SizePages; i++, PhysicalAddress += PAGE_SIZE, VirtualAddress += PAGE_SIZE)
	{
		if (!MiMapPhysicalPage(PageMap, PhysicalAddress, VirtualAddress, PermissionsAndCaching))
		{
			SizePages = i;
			goto Rollback;
		}
	}
	
	MmUnlockKernelSpace();
	return Space;

Rollback:
	MiUnmapPages(PageMap, (uintptr_t) Space, SizePages);
	MmUnlockKernelSpace();
	MmFreePoolBig(Space);
	return NULL;
}
