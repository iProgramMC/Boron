/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/view.c
	
Abstract:
	This module defines functions that map and unmap views
	of files or sections into the virtual memory space.
	
Author:
	iProgramInCpp - 3 April 2025
***/
#include "mi.h"
#include <ex.h>
#include <io.h>

// NOTE: AllocationType and Protection have been validated.
static BSTATUS MmpMapViewOfObject(
	void* MappableObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	BSTATUS Status;

	MmVerifyMappableHeader(MappableObject);
	
	bool IncreaseRefcount = true;
	
	// Provide the state of MEM_SHARED in regards to the state of MEM_COW.
	if (AllocationType & MEM_COW)
		AllocationType &= ~MEM_SHARED;
	else
		AllocationType |= MEM_SHARED;
	
	PMMVAD Vad;
	PMMVAD_LIST VadList;
	
	void* BaseAddress = *BaseAddressInOut;
	BaseAddress = (void*)((uintptr_t)BaseAddress & ~(PAGE_SIZE - 1));
	
	size_t PageOffset = SectionOffset & (PAGE_SIZE - 1);
	size_t ViewSizePages = (ViewSize + PageOffset + PAGE_SIZE - 1) / PAGE_SIZE;
	
	// Increment the reference count of the mappable object.
	if (IncreaseRefcount) {
		ObReferenceObjectByPointer(MappableObject);
	}
	
	// Reserve the region, and then mark it as committed ourselves.
	Status = MmReserveVirtualMemoryVad(
		ViewSizePages,
		AllocationType | MEM_RESERVE | MEM_COMMIT,
		Protection,
		BaseAddress,
		MappableObject,
		SectionOffset & ~(PAGE_SIZE - 1),
		&Vad,
		&VadList
	);
	
	if (FAILED(Status))
	{
		ObDereferenceObject(MappableObject);
		return Status;
	}
	
	*BaseAddressInOut = (void*) Vad->Node.StartVa + PageOffset;
	MmUnlockVadList(VadList);
	
	return STATUS_SUCCESS;
}

//
// Maps a view of either a file or a section.  The type will be checked inside.
// The only supported types of object are MmSectionType and IoFileType.
//
// Parameters:
//     MappedObject - The object of which a view is to be mapped.
//
//     BaseAddressInOut - The base address of the view.  If this is not NULL, specified, then the address
//                        will be read from this parameter.
//
//     ViewSize - The size of the view in bytes.  This pointer will be accessed to store the
//                     size of the view after its creation.
//
//     AllocationType - The type of allocation.  MEM_COMMIT, MEM_TOP_DOWN, MEM_COW, MEM_FIXED and
//                      MEM_OVERRIDE are the allowed flags.
//
//     SectionOffset - The offset within the file or section.  If this isn't aligned to a page boundary,
//                     then neither will the output base address.
//
//     Protection - The protection applied to the pages to be committed.  See OSAllocateVirtualMemory for
//                  more information.
//
BSTATUS MmMapViewOfObject(
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
	
	if (AllocationType & ~(MEM_COMMIT | MEM_TOP_DOWN | MEM_COW | MEM_FIXED | MEM_OVERRIDE))
		return STATUS_INVALID_PARAMETER;
	
	if (!ViewSize)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	void* MappableObject = NULL;
	
	Status = ObReferenceObjectByHandle(MappedObject, IoFileType, &MappableObject);
	if (SUCCEEDED(Status))
	{
		// You cannot map files that are not seekable.
		PFILE_OBJECT FileObject = MappableObject;
		if (!IoIsSeekable(FileObject->Fcb))
		{
			ObDereferenceObject(FileObject);
			return STATUS_UNSUPPORTED_FUNCTION;
		}
	}
	else if (Status == STATUS_TYPE_MISMATCH)
	{
		Status = ObReferenceObjectByHandle(MappedObject, MmSectionObjectType, &MappableObject);
	}
	
	if (FAILED(Status))
		return Status;
	
	Status = MmpMapViewOfObject(
		MappableObject,
		BaseAddressInOut,
		ViewSize,
		AllocationType,
		SectionOffset,
		Protection
	);
	
	ObDereferenceObject(MappableObject);
	return Status;
}

// Allows the duplication of a file handle based on the address it is mapped at.
BSTATUS OSGetMappedFileHandle(
	PHANDLE OutHandle,
	HANDLE ProcessHandle,
	uintptr_t Address
)
{
	void* TargetProcessV;
	PEPROCESS TargetProcess;
	BSTATUS Status;
	
	Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, &TargetProcessV);
	if (FAILED(Status))
		return Status;
	
	TargetProcess = TargetProcessV;
	PEPROCESS OldProcess = PsSetAttachedProcess(TargetProcess);
	
	PMMVAD_LIST VadList = MmLockVadList();
	PMMVAD Vad = MmLookUpVadByAddress(VadList, Address);
	
	if (!Vad)
	{
		Status = STATUS_MEMORY_NOT_RESERVED;
	ReturnEarlyUnlockDetach:
		MmUnlockVadList(VadList);
		PsSetAttachedProcess(OldProcess);
		goto ReturnEarly;
	}
	
	void* FileObject = MmGetBackingObjectView(Vad->View);
	MmVerifyMappableHeader(FileObject);
	
	// TODO: If you still need overlays, uncomment this
	/*
	while (ObGetObjectType(BackingObject) == MmOverlayObjectType)
	{
		PMMOVERLAY Overlay = BackingObject;
		BackingObject = ObReferenceObjectByPointer(Overlay->Parent);
		ObDereferenceObject(Overlay);
	}
	*/
	
	if (ObGetObjectType(FileObject) != IoFileType)
	{
		DbgPrint("Type mismatch in OSGetMappedFileHandle(%p)!", Address);
		DbgPrint("\tIoFileType:               %p", IoFileType);
		DbgPrint("\tMmOverlayObjectType:      %p", MmOverlayObjectType);
		DbgPrint("\tMmSectionObjectType:      %p", MmSectionObjectType);
		DbgPrint("\tThe object's actual type: %p", ObGetObjectType(FileObject));
		Status = STATUS_TYPE_MISMATCH;
		goto ReturnEarlyUnlockDetach;
	}
	
	ObReferenceObjectByPointer(FileObject);
	
	MmUnlockVadList(VadList);
	PsSetAttachedProcess(OldProcess);
	
	// Open the file object as a handle.
	HANDLE FileHandle = 0;
	Status = ObInsertObject(FileObject, &FileHandle, 0);
	ObDereferenceObject(FileObject);
	
	if (FAILED(Status))
		goto ReturnEarly;
	
	// Finally, copy the handle.
	Status = MmSafeCopy(OutHandle, &FileHandle, sizeof(HANDLE), KeGetPreviousMode(), true);
	if (FAILED(Status))
		ObClose(FileHandle);
	
ReturnEarly:
	ObDereferenceObject(TargetProcessV);
	return Status;
}

// Copies data from the current process' VA space and the SourceAddress pointer,
// into a process' virtual address space at TargetAddress.
BSTATUS OSWriteVirtualMemory(HANDLE ProcessHandle, void* TargetAddress, const void* SourceAddress, size_t ByteCount)
{
	BSTATUS Status;
	
	// Reference the current process handle.
	void* TargetProcessV;
	PEPROCESS TargetProcess;
	Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, &TargetProcessV);
	if (FAILED(Status))
		return Status;
	
	TargetProcess = TargetProcessV;
	
	// Allocate an MDL for the current process' source memory.
	PMDL Mdl = MmAllocateMdl((uintptr_t) SourceAddress, ByteCount);
	if (!Mdl)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail0;
	}
	
	Status = MmProbeAndPinPagesMdl(Mdl, KeGetPreviousMode(), false);
	if (FAILED(Status))
		goto Fail1;
	
	// Map this MDL into system memory.
	void* SourceAddressMapped = NULL;
	Status = MmMapPinnedPagesMdl(Mdl, &SourceAddressMapped);
	if (FAILED(Status))
		goto Fail2;
	
	// Now perform the copy.
	PEPROCESS OldProcess = PsSetAttachedProcess(TargetProcess);
	Status = MmSafeCopy(TargetAddress, SourceAddressMapped, ByteCount, KeGetPreviousMode(), true);
	PsSetAttachedProcess(OldProcess);
	
	MmUnmapPagesMdl(Mdl);
Fail2:
	MmUnpinPagesMdl(Mdl);
Fail1:
	MmFreeMdl(Mdl);
Fail0:
	ObDereferenceObject(TargetProcess);
	return Status;
}

// Copies data from a process' VA space at the SourceAddress, to the current process'
// VA space at the TargetAddress.
BSTATUS OSReadVirtualMemory(HANDLE ProcessHandle, void* TargetAddress, const void* SourceAddress, size_t ByteCount)
{
	BSTATUS Status;
	
	// Reference the current process handle.
	void* TargetProcessV;
	PEPROCESS TargetProcess;
	Status = ExReferenceObjectByHandle(ProcessHandle, PsProcessObjectType, &TargetProcessV);
	if (FAILED(Status))
		return Status;
	
	TargetProcess = TargetProcessV;
	
	// Allocate an MDL for the current process' destination memory.
	PMDL Mdl = MmAllocateMdl((uintptr_t) TargetAddress, ByteCount);
	if (!Mdl)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail0;
	}
	
	Status = MmProbeAndPinPagesMdl(Mdl, KeGetPreviousMode(), false);
	if (FAILED(Status))
		goto Fail1;
	
	// Map this MDL into system memory.
	void* DestinationAddress = NULL;
	Status = MmMapPinnedPagesMdl(Mdl, &DestinationAddress);
	if (FAILED(Status))
		goto Fail2;
	
	// Now perform the copy.
	PEPROCESS OldProcess = PsSetAttachedProcess(TargetProcess);
	Status = MmSafeCopy(DestinationAddress, SourceAddress, ByteCount, KeGetPreviousMode(), false);
	PsSetAttachedProcess(OldProcess);
	
	MmUnmapPagesMdl(Mdl);
Fail2:
	MmUnpinPagesMdl(Mdl);
Fail1:
	MmFreeMdl(Mdl);
Fail0:
	ObDereferenceObject(TargetProcess);
	return Status;
}
