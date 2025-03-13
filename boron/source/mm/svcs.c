/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/svcs.c
	
Abstract:
	This module defines system service functions pertaining
	to the system's memory manager.
	
Author:
	iProgramInCpp - 5 March 2025
***/
#include <mm.h>
#include <ex.h>
#include "mi.h"

//
// Either reserves, commits, or reserves and commits, a region of virtual memory.
//
// Parameters:
//     BaseAddressInOut - The base address of the region.
//
//     RegionSizeInOut  - The region's size.
//
//     AllocationType   - One of MEM_COMMIT, MEM_RESERVE, or MEM_COMMIT | MEM_RESERVE.
//
//     Protection       - The protection applied to the pages to be committed.
//
//
// Operation:
//   If AllocationType is MEM_RESERVE, then BaseAddressInOut may be NULL, in which case the
//   address of the reserved region is determined automatically. Otherwise, the address is
//   loaded from BaseAddressInOut, and rounded down to the lowest page size boundary.
//
//   If AllocationType is MEM_COMMIT and not MEM_RESERVE, then BaseAddressInOut may not be
//   NULL.  The start address of the region to commit is loaded from BaseAddressInOut,
//   and rounded down to the lowest page size boundary.
//
//   If AllocationType is MEM_COMMIT and MEM_RESERVE, then BaseAddressInOut behaves the same
//   as in MEM_RESERVE's case, except the round down rule conforms to MEM_COMMIT's rule.
//   (The rule for MEM_RESERVE may change in the future.)
//
//   If AllocationType is MEM_COMMIT and not MEM_RESERVE, then Protection may be 0, in which
//   case the pages will take the Protection from the reserved region.  One of them must be
//   non zero.
//
BSTATUS OSAllocateVirtualMemory(
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,
	int Protection
)
{
	// Check parameters.
	if (Protection & ~(PAGE_READ | PAGE_WRITE | PAGE_EXECUTE))
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & ~(MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN | MEM_SHARED))
		return STATUS_INVALID_PARAMETER;
	
	// One of these needs to be set.
	if (~AllocationType & (MEM_COMMIT | MEM_RESERVE))
		return STATUS_INVALID_PARAMETER;
	
	bool HasAddressPointer = BaseAddressInOut != NULL;
	void* BaseAddress = NULL;
	size_t RegionSize = 0;
	BSTATUS Status;
	
	if (HasAddressPointer)
	{
		Status = MmSafeCopy(&BaseAddress, BaseAddressInOut, sizeof(void*), KeGetPreviousMode(), false);
		if (FAILED(Status))
			return Status;
	}
	
	Status = MmSafeCopy(&RegionSize, RegionSizeInOut, sizeof(size_t), KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	size_t SizePages = (RegionSize + PAGE_SIZE - 1) / PAGE_SIZE;
	if (SizePages == 0)
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & MEM_RESERVE)
	{
		// TODO: Receiving a base address.
		if (HasAddressPointer)
		{
			ASSERT(!"TODO");
			return STATUS_UNIMPLEMENTED;
		}
		
		// NOTE: AllocationType & MEM_COMMIT won't be ignored. It marks the VAD as committed.
		Status = MmReserveVirtualMemory(SizePages, &BaseAddress, AllocationType, Protection);
	}
	else if (AllocationType & MEM_COMMIT)
	{
		if (!HasAddressPointer)
			return STATUS_INVALID_PARAMETER;
		
		Status = MmCommitVirtualMemory((uintptr_t) BaseAddress, SizePages, Protection);
	}
	
	RegionSize = SizePages * PAGE_SIZE;
	
	if (SUCCEEDED(Status))
	{
		Status = MmSafeCopy(BaseAddressInOut, &BaseAddress, sizeof(void*), KeGetPreviousMode(), true);
		if (FAILED(Status))
			return Status;
		
		Status = MmSafeCopy(RegionSizeInOut, &RegionSize, sizeof(size_t), KeGetPreviousMode(), true);
	}
	
	return Status;
}

//
// Either decommits or releases a region of virtual memory.
//
//
// Parameters:
//     BaseAddressInOut - The base address of the region.
//
//     RegionSizeInOut  - The region's size.
//
//     FreeType         - One of MEM_DECOMMIT or MEM_RELEASE.
//
// Operation:
//   BaseAddressInOut and RegionSizeInOut may not be NULL.
//
//   If FreeType is MEM_DECOMMIT, the memory region is only decommitted and not released.
//
//   If FreeType is MEM_RELEASE, the memory region is released.  If MEM_DECOMMIT isn't set,
//   the region passed in as parameter must not be committed in any place.  Otherwise,
//   the committed regions are decommitted before releasing the memory region.
//
//   If MEM_DECOMMIT | MEM_RELEASE is specified, but the memory range does not cover an
//   entire reserved region, the memory will still be decommitted, but an error status
//   will be returned.
//
BSTATUS OSFreeVirtualMemory(
	void* BaseAddress,
	size_t RegionSize,
	int FreeType
)
{
	// Check parameters.
	if (FreeType & ~(MEM_DECOMMIT | MEM_RELEASE))
		return STATUS_INVALID_PARAMETER;
	
	// One of these must be set.
	if (~FreeType & (MEM_DECOMMIT | MEM_RELEASE))
		return STATUS_INVALID_PARAMETER;
	
	// But not both.
	if ((FreeType & (MEM_DECOMMIT | MEM_RELEASE)) == (MEM_DECOMMIT | MEM_RELEASE))
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	
	size_t SizePages = (RegionSize + PAGE_SIZE - 1) / PAGE_SIZE;
	if (SizePages == 0)
		return STATUS_INVALID_PARAMETER;
	
	Status = MmDecommitVirtualMemory((uintptr_t) BaseAddress, SizePages);
	if (FAILED(Status))
		return Status;
	
	if (FreeType == MEM_RELEASE)
	{
		Status = MmReleaseVirtualMemory(BaseAddress);
	}
	
	return Status;
}
