/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/sview.c

Abstract:
	This module defines functions that map and unmap views
	of files or sections into the system's VM space.

Author:
	iProgramInCpp - 4 June 2025
***/
#include "mi.h"
#include <ex.h>
#include <io.h>
#include <cc.h>

BSTATUS MmMapViewOfFileInSystemSpace(
	PFCB Fcb,
	void** BaseAddressOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	BSTATUS Status;

	// You cannot map files that are not seekable.
	if (!IoIsSeekable(Fcb))
		return STATUS_UNSUPPORTED_FUNCTION;

	// Allocate a file object for this mapping.
	// TODO: Optimize this?
	PFILE_OBJECT FileObject;
	Status = IoCreateFileObject(Fcb, &FileObject, 0);
	if (FAILED(Status))
		return Status;

	size_t PageOffset = SectionOffset & (PAGE_SIZE - 1);
	size_t ViewSizePages = (ViewSize + PageOffset + PAGE_SIZE - 1) / PAGE_SIZE;

	PMMVAD Vad = NULL;
	PMMVAD_LIST VadList = &MiSystemVadList;

	CcPurgeViewsOverLimit(1);

	// System view space mappings CANNOT be private.
	AllocationType &= ~MEM_COW;
	AllocationType |= MEM_SHARED;

	// First, allocate the space.
	void* StartAddress = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, ViewSizePages, POOL_TAG("MmVw"));
	if (!StartAddress)
	{
		ObDereferenceObject(FileObject);
		return STATUS_INSUFFICIENT_MEMORY;
	}

	uint64_t VadSectionOffset = SectionOffset & ~(PAGE_SIZE - 1);

	// Then try to initialize and add the VAD.
	Status = MiInitializeAndInsertVad(
		VadList,
		&Vad,
		StartAddress,
		ViewSizePages,
		AllocationType,
		Protection,
		false,
		FileObject,
		VadSectionOffset,
		NULL,  // ViewOverride
		0      // ViewOffset
	);

	// MiInitializeAndInsertVad references the FileObject.  So we shouldn't keep our reference here.
	ObDereferenceObject(FileObject);

	if (FAILED(Status))
	{
		// Deallocate the space we allocated.
		MmFreePoolBig(StartAddress);
		return Status;
	}

	uint64_t NewSectionOffset = SectionOffset & ~(PAGE_SIZE - 1);
	Vad->ViewCacheEntry.Key = (uintptr_t)(NewSectionOffset / VIEW_CACHE_SIZE);

	*BaseAddressOut = (void*) Vad->Node.StartVa + PageOffset;

	MmUnlockVadList(VadList);

	// Add this VAD into the FCB's view cache.
	ExAcquireFastMutex(&Fcb->CacheInfo.ViewCacheMutex, false);
	InsertItemRbTree(&Fcb->CacheInfo.ViewCache, &Vad->ViewCacheEntry);
	ExReleaseFastMutex(&Fcb->CacheInfo.ViewCacheMutex);

	CcAddVadToViewCacheLru(Vad);
	return STATUS_SUCCESS;
}

void MmUnmapViewOfFileInSystemSpace(void* ViewPointer, bool RemoveFromFcbViewCache)
{
	// First of all, fetch the VAD from this pointer.
	MiLockVadList(&MiSystemVadList);

	DbgPrint("MmUnmapViewOfFileInSystemSpace(%p)", ViewPointer);

	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&MiSystemVadList.Tree, (uintptr_t) ViewPointer);
	ASSERT(Entry && "Bad pointer passed into MmUnmapViewOfFileInSystemSpace");

	PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);

	// Remove it from the list of VADs.
	RemoveItemRbTree(&MiSystemVadList.Tree, &Vad->Node.Entry);

	// Decommit the VAD.
	MiDecommitVadInSystemSpace(Vad);

	MmUnlockVadList(&MiSystemVadList);

	// Remove the VAD from the FCB's view cache.
	if (RemoveFromFcbViewCache)
	{
		PFILE_OBJECT FileObject = Vad->View->BackingObject;
		ASSERT(ObGetObjectType(FileObject) == IoFileType);

		PFCB Fcb = FileObject->Fcb;
		ExAcquireFastMutex(&Fcb->CacheInfo.ViewCacheMutex, false);
		RemoveItemRbTree(&Fcb->CacheInfo.ViewCache, &Vad->ViewCacheEntry);
		ExReleaseFastMutex(&Fcb->CacheInfo.ViewCacheMutex);
	}

	// Then, clean up.
	MiCleanUpVad(Vad);

	// Remove the VAD from the LRU list.
	CcRemoveVadFromViewCacheLru(Vad);

	// Finally, relinquish the address space we occupied.
	MmFreePoolBig(ViewPointer);
}
