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
BSTATUS MmMapViewOfFile(
	PFILE_OBJECT FileObject,
	void** BaseAddressInOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	// You cannot map files that are not seekable.
	if (!IoIsSeekable(FileObject->Fcb))
		return STATUS_UNSUPPORTED_FUNCTION;
	
	
	PMMVAD Vad;
	PMMVAD_LIST VadList;
	
	void* BaseAddress = *BaseAddressInOut;
	BaseAddress = (void*)((uintptr_t)BaseAddress & ~(PAGE_SIZE - 1));
	
	size_t PageOffset = SectionOffset & (PAGE_SIZE - 1);
	size_t ViewSizePages = (ViewSize + PageOffset + PAGE_SIZE - 1) / PAGE_SIZE;
	
	// Reserve the region, and then mark it as committed ourselves.
	BSTATUS Status = MmReserveVirtualMemoryVad(ViewSizePages, AllocationType | MEM_RESERVE, Protection, BaseAddress, &Vad, &VadList);
	if (FAILED(Status))
		return Status;
	
	// Vad Protection and Private are filled in by MmReserveVirtualMemoryVad.
	Vad->Flags.Committed = 1;
	Vad->Flags.IsFile = 1;
	Vad->Mapped.FileObject = ObReferenceObjectByPointer(FileObject);
	Vad->SectionOffset = SectionOffset & ~(PAGE_SIZE - 1);
	
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
//     AllocationType - The type of allocation.  MEM_TOP_DOWN and MEM_SHARED are the allowed flags.
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
	
	if (AllocationType & ~(MEM_COMMIT | MEM_SHARED | MEM_TOP_DOWN | MEM_COW))
		return STATUS_INVALID_PARAMETER;
	
	if (!ViewSize)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	PFILE_OBJECT FileObject = NULL;
	//PMMSECTION SectionObject = NULL;
	
	Status = ObReferenceObjectByHandle(MappedObject, IoFileType, (void**) &FileObject);
	if (SUCCEEDED(Status))
	{
		Status = MmMapViewOfFile(
			FileObject,
			BaseAddressInOut,
			ViewSize,
			AllocationType,
			SectionOffset,
			Protection
		);
		
		ObDereferenceObject(FileObject);
		return Status;
	}
	
	/*
	TODO:
	
	Status = ObReferenceObjectByHandle(MappedObject, MmSectionType, (void**) &FileObject);
	if (SUCCEEDED(Status))
	{
		Status = MmMapViewOfSection(
			FileObject,
			BaseAddressInOut,
			ViewSizeInOut,
			AllocationType,
			SectionOffset,
			Protection
		);
		
		ObDereferenceObject(FileObject);
		return Status;
	}
	*/
	
	return STATUS_TYPE_MISMATCH;
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
	
	if (!Vad || !Vad->Flags.IsFile)
	{
		Status = STATUS_MEMORY_NOT_RESERVED;
		MmUnlockVadList(VadList);
		PsSetAttachedProcess(OldProcess);
		goto ReturnEarly;
	}
	
	void* FileObject = Vad->Mapped.FileObject;
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

BSTATUS OSWriteVirtualMemory(HANDLE ProcessHandle, void* TargetAddress, const void* Source, size_t ByteCount)
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
	PMDL Mdl = MmAllocateMdl((uintptr_t) Source, ByteCount);
	if (!Mdl)
	{
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Fail0;
	}
	
	Status = MmProbeAndPinPagesMdl(Mdl, KeGetPreviousMode(), false);
	if (FAILED(Status))
		goto Fail1;
	
	// Map this MDL into system memory.
	void* SourceAddress = NULL;
	Status = MmMapPinnedPagesMdl(Mdl, &SourceAddress);
	if (FAILED(Status))
		goto Fail2;
	
	// Now perform the copy.
	PEPROCESS OldProcess = PsSetAttachedProcess(TargetProcess);
	Status = MmSafeCopy(TargetAddress, SourceAddress, ByteCount, KeGetPreviousMode(), true);
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
