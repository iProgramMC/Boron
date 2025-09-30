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
#include <ps.h>
#include "mi.h"

//
// Either reserves, commits, or reserves and commits, a region of virtual memory.
//
// Parameters:
//     ProcessHandle - The handle to process to modify the virtual address space for.
//
//     BaseAddressInOut - The base address of the region.
//
//     RegionSizeInOut  - The region's size.
//
//     AllocationType   - A combination of flags: MEM_COMMIT, MEM_RESERVE, MEM_TOP_DOWN and/or MEM_SHARED.
//
//     Protection       - The protection applied to the pages to be committed.
//
//
// Operation:
//   If AllocationType is MEM_RESERVE, then BaseAddressInOut may point to NULL, in which case
//   the address of the reserved region is determined automatically. Otherwise, the address is
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
	HANDLE ProcessHandle,
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
	
	// You must pass in a handle, even if it is CURRENT_PROCESS_HANDLE.
	if (!ProcessHandle)
		return STATUS_INVALID_PARAMETER;
	
	void* BaseAddress = NULL;
	size_t RegionSize = 0;
	BSTATUS Status;
	
	Status = MmSafeCopy(&BaseAddress, BaseAddressInOut, sizeof(void*), KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	Status = MmSafeCopy(&RegionSize, RegionSizeInOut, sizeof(size_t), KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	size_t SizePages = (RegionSize + PAGE_SIZE - 1) / PAGE_SIZE;
	if (SizePages == 0)
		return STATUS_INVALID_PARAMETER;
	
	PEPROCESS Process = NULL;
	PEPROCESS ProcessRestore = NULL;
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
		if (FAILED(Status))
			return Status;
		
		ProcessRestore = PsSetAttachedProcess(Process);
	}
	
	bool HasAddressPointer = BaseAddress != NULL;
	
	if (AllocationType & MEM_RESERVE)
	{
		// NOTE: AllocationType & MEM_COMMIT won't be ignored. It marks the VAD as committed.
		Status = MmReserveVirtualMemory(SizePages, &BaseAddress, AllocationType, Protection);
		
	}
	else if (AllocationType & MEM_COMMIT)
	{
		if (HasAddressPointer)
			Status = MmCommitVirtualMemory((uintptr_t) BaseAddress, SizePages, Protection);
		else
			Status = STATUS_INVALID_PARAMETER;
	}
	
	RegionSize = SizePages * PAGE_SIZE;
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		PsSetAttachedProcess(ProcessRestore);
		ObDereferenceObject(Process);
	}
	
	if (SUCCEEDED(Status))
	{
		Status = MmSafeCopy(BaseAddressInOut, &BaseAddress, sizeof(void*), KeGetPreviousMode(), true);
		
		if (SUCCEEDED(Status))
			Status = MmSafeCopy(RegionSizeInOut, &RegionSize, sizeof(size_t), KeGetPreviousMode(), true);
	}
	
	return Status;
}

//
// Either decommits or releases a region of virtual memory.
//
//
// Parameters:
//     ProcessHandle - The handle to process to modify the virtual address space for.
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
//   If FreeType is MEM_RELEASE, the memory region is first decommitted, and then released.
//
//   If MEM_DECOMMIT | MEM_RELEASE is specified, an error will be returned.
//
//   If BaseAddress is not aligned to the page, then it will be rounded to the lowest
//   virtual page aligned address and the region's size will be increased by the reported
//   misalignment value.
//
BSTATUS OSFreeVirtualMemory(
	HANDLE ProcessHandle,
	void* BaseAddress,
	size_t RegionSize,
	int FreeType
)
{
	// Check parameters.
	if (FreeType & ~(MEM_DECOMMIT | MEM_RELEASE))
		return STATUS_INVALID_PARAMETER;
	
	// One of these must be set.
	if ((FreeType & (MEM_DECOMMIT | MEM_RELEASE)) == 0)
		return STATUS_INVALID_PARAMETER;
	
	// But not both.
	if ((FreeType & (MEM_DECOMMIT | MEM_RELEASE)) == (MEM_DECOMMIT | MEM_RELEASE))
		return STATUS_INVALID_PARAMETER;
	
	// You must pass in a handle, even if it is CURRENT_PROCESS_HANDLE.
	if (!ProcessHandle)
		return STATUS_INVALID_PARAMETER;
	
	RegionSize += (size_t) ((uintptr_t)BaseAddress & (PAGE_SIZE - 1));
	BaseAddress = (void*) ((uintptr_t)BaseAddress & ~(PAGE_SIZE - 1));
	
	BSTATUS Status;
	
	size_t SizePages = (RegionSize + PAGE_SIZE - 1) / PAGE_SIZE;
	if (SizePages == 0)
		return STATUS_INVALID_PARAMETER;
	
	PEPROCESS Process = NULL;
	PEPROCESS ProcessRestore = NULL;
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
		if (FAILED(Status))
			return Status;
		
		ProcessRestore = PsSetAttachedProcess(Process);
	}
	
	Status = MmDecommitVirtualMemory((uintptr_t) BaseAddress, SizePages);
	if (SUCCEEDED(Status))
	{
		if (FreeType == MEM_RELEASE)
			Status = MmReleaseVirtualMemory(BaseAddress);
	}
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		PsSetAttachedProcess(ProcessRestore);
		ObDereferenceObject(Process);
	}
	return Status;
}

//
// Maps a view of either a file or a section.  The type will be checked inside.
// The only supported types of object are MmSectionType and IoFileType.
//
// Parameters:
//     ProcessHandle - The handle to process into which to map the file.
//
//     MappedObject - The object of which a view is to be mapped.
//
//     BaseAddressOut - The base address of the view.  If this is not NULL, and the value that this
//                      points to is also not NULL, then the address will be used as the mapping base
//                      instead.  Mapping to address zero is not allowed.
//
//     ViewSize - The size of the view in bytes.  This pointer will be accessed to store the
//                size of the view after its creation.
//
//     AllocationType - The type of allocation.  MEM_TOP_DOWN and MEM_SHARED are the allowed flags.
//
//     SectionOffset - The offset within the file or section.  If this isn't aligned to a page boundary,
//                     then neither will the output base address.
//
//     Protection - The protection applied to the pages to be committed.  See OSAllocateVirtualMemory for
//                  more information.
//
BSTATUS OSMapViewOfObject(
	HANDLE ProcessHandle,
	HANDLE MappedObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	if (Protection & ~(PAGE_READ | PAGE_WRITE | PAGE_EXECUTE))
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & ~(MEM_COMMIT | MEM_SHARED | MEM_TOP_DOWN | MEM_COW))
		return STATUS_INVALID_PARAMETER;
	
	if (!ViewSize)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	PEPROCESS Process = NULL, ProcessRestore = NULL;
	void* BaseAddress = NULL;
	
	Status = MmSafeCopy(&BaseAddress, BaseAddressInOut, sizeof(void*), KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, (void**) &Process);
		if (FAILED(Status))
			return Status;
		
		ProcessRestore = PsSetAttachedProcess(Process);
	}
	
	Status = MmMapViewOfObject(
		MappedObject,
		&BaseAddress,
		ViewSize,
		AllocationType,
		SectionOffset,
		Protection
	);
	
	if (ProcessHandle != CURRENT_PROCESS_HANDLE)
	{
		PsSetAttachedProcess(ProcessRestore);
		ObDereferenceObject(Process);
	}
	
	if (SUCCEEDED(Status))
		Status = MmSafeCopy(BaseAddressInOut, &BaseAddress, sizeof(void*), KeGetPreviousMode(), true);
	
	return Status;
}
