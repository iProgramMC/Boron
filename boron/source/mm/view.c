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
	void** BaseAddressOut,
	size_t* ViewSizeInOut,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	// You cannot map files that are not seekable.
	if (!IoIsSeekable(FileObject))
		return STATUS_UNSUPPORTED_FUNCTION;
	
	size_t ViewSize = *ViewSizeInOut;
	size_t PageOffset = SectionOffset & (PAGE_SIZE - 1);
	size_t ViewSizePages = (ViewSize + PageOffset + PAGE_SIZE - 1) / PAGE_SIZE;
	
	PMMVAD Vad;
	PMMVAD_LIST VadList;
	
	// Reserve the region, and then mark it as committed ourselves.
	BSTATUS Status = MmReserveVirtualMemoryVad(ViewSizePages, AllocationType | MEM_RESERVE, Protection, &Vad, &VadList);
	if (FAILED(Status))
		return Status;
	
	// Vad Protection and Private are filled in by MmReserveVirtualMemoryVad.
	Vad->Flags.Committed = 1;
	Vad->Flags.IsFile = 1;
	Vad->Mapped.FileObject = ObReferenceObjectByPointer(FileObject);
	Vad->SectionOffset = SectionOffset & ~(PAGE_SIZE - 1);
	
	*BaseAddressOut = (void*) Vad->Node.StartVa;
	*ViewSizeInOut = PAGE_SIZE * ViewSizePages;
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
//     BaseAddressOut - The base address of the view.  If MEM_FIXED is specified, then the address
//                      will be read from this parameter.
//
//     ViewSizeInOut - The size of the view in bytes.  This pointer will be accessed to store the
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
	void** BaseAddressOut,
	size_t* ViewSizeInOut,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	if (Protection & ~(PAGE_READ | PAGE_WRITE | PAGE_EXECUTE))
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & ~(MEM_COMMIT | MEM_SHARED | MEM_TOP_DOWN))
		return STATUS_INVALID_PARAMETER;
	
	if (!ViewSizeInOut)
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status;
	PFILE_OBJECT FileObject = NULL;
	//PMMSECTION SectionObject = NULL;
	
	Status = ObReferenceObjectByHandle(MappedObject, IoFileType, (void**) &FileObject);
	if (SUCCEEDED(Status))
	{
		Status = MmMapViewOfFile(
			FileObject,
			BaseAddressOut,
			ViewSizeInOut,
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
			BaseAddressOut,
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
