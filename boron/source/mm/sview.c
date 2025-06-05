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
	PFILE_OBJECT FileObject,
	void** BaseAddressOut,
	size_t ViewSize,
	int AllocationType,
	uint64_t SectionOffset,
	int Protection
)
{
	// You cannot map files that are not seekable.
	if (!IoIsSeekable(FileObject->Fcb))
		return STATUS_UNSUPPORTED_FUNCTION;
	
	size_t PageOffset = SectionOffset & (PAGE_SIZE - 1);
	size_t ViewSizePages = (ViewSize + PageOffset + PAGE_SIZE - 1) / PAGE_SIZE;
	
	PMMVAD Vad = NULL;
	PMMVAD_LIST VadList = &MiSystemVadList;
	
	CcPurgeViewsOverLimit(1);
	
	// First, allocate the space.
	void* StartAddress = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, ViewSizePages, POOL_TAG("MmVw"));
	if (!StartAddress)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Then try to initialize and add the VAD.
	BSTATUS Status = MiInitializeAndInsertVad(
		VadList,
		&Vad,
		StartAddress,
		ViewSizePages,
		AllocationType,
		Protection,
		false
	);
	
	if (FAILED(Status))
	{
		// Deallocate the space we allocated.
		MmFreePoolBig(StartAddress);
		return Status;
	}
	
	// Vad Protection and Private are filled in by MiInitializeAndInsertVad.
	Vad->Flags.Committed = 1;
	Vad->Flags.IsFile = 1;
	Vad->Mapped.FileObject = ObReferenceObjectByPointer(FileObject);
	Vad->SectionOffset = SectionOffset & ~(PAGE_SIZE - 1);
	Vad->ViewCacheEntry.Key = Vad->SectionOffset;
	
	*BaseAddressOut = (void*) Vad->Node.StartVa + PageOffset;
	
	MmUnlockVadList(VadList);
	
	// Add this VAD into the FCB's view cache.
	PFCB Fcb = FileObject->Fcb;
	
	KeWaitForSingleObject(&Fcb->ViewCacheMutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	InsertItemRbTree(&Fcb->ViewCache, &Vad->ViewCacheEntry);
	KeReleaseMutex(&Fcb->ViewCacheMutex);
	
	CcAddVadToViewCacheLru(Vad);
	return STATUS_SUCCESS;
}

void MmUnmapViewOfFileInSystemSpace(void* ViewPointer, bool RemoveFromFcbViewCache)
{
	// First of all, fetch the VAD from this pointer.
	MiLockVadList(&MiSystemVadList);
	
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
		PFILE_OBJECT FileObject = Vad->Mapped.FileObject;
		ASSERT(ObGetObjectType(FileObject) == IoFileType);
		
		PFCB Fcb = FileObject->Fcb;
		KeWaitForSingleObject(&Fcb->ViewCacheMutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
		RemoveItemRbTree(&Fcb->ViewCache, &Vad->ViewCacheEntry);
		KeReleaseMutex(&Fcb->ViewCacheMutex);
	}
	
	// Then, clean up.
	MiCleanUpVad(Vad);
	
	// Remove the VAD from the LRU list.
	CcRemoveVadFromViewCacheLru(Vad);
	
	// Finally, relinquish the address space we occupied.
	MmFreePoolBig(ViewPointer);
}
