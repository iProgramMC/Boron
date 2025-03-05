/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/calls.c
	
Abstract:
	This module defines system service functions pertaining
	to the system's memory manager.
	
Author:
	iProgramInCpp - 5 March 2025
***/
#include <mm.h>
#include <ex.h>

//
// Either reserves, commits, or reserves and commits, a region of virtual memory.
//
// Parameters:
//     ProcessObject - The handle to the process whose virtual memory is to be modified.
//                     Note that the current process must have the permissions required
//                     to modify the virtual address space of the process object.
//
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
BSTATUS OSAllocateVirtualMemory(
	HANDLE ProcessObject,
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,         // either MEM_RESERVE, MEM_COMMIT, or MEM_COMMIT | MEM_RESERVE
	int Protection              // protection type (R, W, X)
)
{
	(void) ProcessObject;
	(void) BaseAddressInOut;
	(void) RegionSizeInOut;
	(void) AllocationType;
	(void) Protection;
	return STATUS_UNIMPLEMENTED;
}

//
// Either decommits or releases a region of virtual memory.
//
//
// Parameters:
//     ProcessObject - The handle to the process whose virtual memory is to be modified.
//                     Note that the current process must have the permissions required
//                     to modify the virtual address space of the process object.
//
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
BSTATUS OSFreeVirtualMemory(
	HANDLE ProcessObject,
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int FreeType
)
{
	(void) ProcessObject;
	(void) BaseAddressInOut;
	(void) RegionSizeInOut;
	(void) FreeType;
	return STATUS_UNIMPLEMENTED;
}
