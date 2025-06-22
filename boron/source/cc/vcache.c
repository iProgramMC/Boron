/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc/vcache.c
	
Abstract:
	This module implements the view cache object for the Boron
	memory manager.
	
Author:
	iProgramInCpp - 4 June 2025
***/
#include "cci.h"
#include <io.h>

// Purge any VADs associated with this FCB.
void CcPurgeViewsForFile(PFCB Fcb)
{
	// NOTE: This is *only* meant to be called when dereferencing
	// the FCB for the last time, and it's about to be wiped out!
	//
	// However we will still protect against race conditions just in case.
	CcAcquireMutex(&Fcb->ViewCacheMutex);
	
	while (!IsEmptyRbTree(&Fcb->ViewCache))
	{
		// Get the first entry and remove it.
		PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&Fcb->ViewCache);
		RemoveItemRbTree(&Fcb->ViewCache, Entry);
		KeReleaseMutex(&Fcb->ViewCacheMutex);
		
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, ViewCacheEntry);
		MmUnmapViewOfFileInSystemSpace((void*) Vad->Node.StartVa, false);
		
		CcAcquireMutex(&Fcb->ViewCacheMutex);
	}
	
	ASSERT(IsEmptyRbTree(&Fcb->ViewCache));
	KeReleaseMutex(&Fcb->ViewCacheMutex);
}

// NOTE: The view cache mutex needs to be locked.
static BSTATUS CciGetViewOfFile(PFILE_OBJECT FileObject, uint64_t FileOffset, void** ViewOut)
{
	PFCB Fcb = FileObject->Fcb;
	ASSERT(Fcb);
	
	BSTATUS Status = STATUS_SUCCESS;
	void* View = NULL;
	
	// First, check if the view already exists.
#ifndef IS_64_BIT
#error TODO: Should we just use 64-bit keys throughout? The RB trees currently have 32-bit keys.
#endif
	
	PRBTREE_ENTRY Entry = LookUpItemRbTree(&Fcb->ViewCache, FileOffset & ~(VIEW_CACHE_SIZE - 1));
	if (Entry)
	{
		// Already exists!  Just return it then.
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, ViewCacheEntry);
		View = (void*) Vad->Node.StartVa;
	}
	else
	{
		// Does not exist.  Try to map it in.
		Status = MmMapViewOfFileInSystemSpace(
			FileObject,
			&View,
			VIEW_CACHE_SIZE,
			MEM_RESERVE | MEM_COMMIT,
			FileOffset & ~(VIEW_CACHE_SIZE - 1),
			PAGE_READ | PAGE_WRITE
		);
	}
	
	if (SUCCEEDED(Status))
		*ViewOut = View;
	
	return Status;
}

// Reads the contents of a file and copies them to a buffer.
//
// NOTE: This will always block, because we cannot control when a page
// fault will block or will not block.
BSTATUS CcReadFileMdl(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	PMDL Mdl,
	uintptr_t MdlOffset,
	size_t ByteCount
)
{
	// TODO: Any possible race conditions?!
	BSTATUS Status;
	size_t Size = ByteCount;
	
	ASSERT(FileObject);
	
	PFCB Fcb = FileObject->Fcb;
	ASSERT(Fcb);
	CcAcquireMutex(&Fcb->ViewCacheMutex);
	
	while (Size)
	{
		size_t ViewOffset = FileOffset % VIEW_CACHE_SIZE;
		
		size_t BytesTillNext = VIEW_CACHE_SIZE - ViewOffset;
		size_t CopyAmount;
		
		if (Size < BytesTillNext)
			CopyAmount = Size;
		else
			CopyAmount = BytesTillNext;
		
		// First of all, look up the view, see if it exists.
		//
		// If it does not exist, CcGetViewOfFile creates one for it.
		void* View = NULL;
		Status = CciGetViewOfFile(FileObject, FileOffset, &View);
		
		if (FAILED(Status))
		{
			KeReleaseMutex(&Fcb->ViewCacheMutex);
			return Status;
		}
		
		// The view exists, so let's copy.
		MmCopyIntoMdl(Mdl, MdlOffset, (char*)View + ViewOffset, CopyAmount);
		
		MdlOffset += CopyAmount;
		FileOffset += CopyAmount;
		Size -= CopyAmount;
	}
	
	KeReleaseMutex(&Fcb->ViewCacheMutex);
	return STATUS_SUCCESS;
}

// Reads the contents of a file and copies them to a buffer.
BSTATUS CcReadFileCopy(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	void* Buffer,
	size_t Size
)
{
	PMDL Mdl;
	BSTATUS Status;
	
	Status = MmCreateMdl(&Mdl, (uintptr_t) Buffer, Size, KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;
	
	Status = CcReadFileMdl(FileObject, FileOffset, Mdl, 0, Size);
	
	MmFreeMdl(Mdl);
	
	return Status;
}
