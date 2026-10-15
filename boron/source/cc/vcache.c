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
	CcAcquireMutex(&Fcb->CacheInfo.ViewCacheMutex);

	while (!IsEmptyRbTree(&Fcb->CacheInfo.ViewCache))
	{
		// Get the first entry and remove it.
		PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&Fcb->CacheInfo.ViewCache);
		RemoveItemRbTree(&Fcb->CacheInfo.ViewCache, Entry);
		KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);

		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, ViewCacheEntry);
		MmUnmapViewOfFileInSystemSpace((void*) Vad->Node.StartVa, false);

		CcAcquireMutex(&Fcb->CacheInfo.ViewCacheMutex);
	}

	ASSERT(IsEmptyRbTree(&Fcb->CacheInfo.ViewCache));
	KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);
}

// NOTE: The view cache mutex needs to be locked.
static BSTATUS CciGetViewOfFile(PFILE_OBJECT FileObject, uint64_t FileOffset, void** ViewOut)
{
	PFCB Fcb = FileObject->Fcb;
	ASSERT(Fcb);

	BSTATUS Status = STATUS_SUCCESS;
	void* View = NULL;

	// First, check if the view already exists.
	PRBTREE_ENTRY Entry = LookUpItemRbTree(&Fcb->CacheInfo.ViewCache, (uintptr_t)(FileOffset / VIEW_CACHE_SIZE));
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
	CcAcquireMutex(&Fcb->CacheInfo.ViewCacheMutex);

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
			KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);
			return Status;
		}

		// The view exists, so let's copy.
		MmCopyIntoMdl(Mdl, MdlOffset, (char*)View + ViewOffset, CopyAmount);

		MdlOffset += CopyAmount;
		FileOffset += CopyAmount;
		Size -= CopyAmount;
	}

	KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);
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

	Status = MmCreateMdl(&Mdl, (uintptr_t) Buffer, Size, KeGetPreviousMode(), true);
	if (FAILED(Status))
		return Status;

	Status = CcReadFileMdl(FileObject, FileOffset, Mdl, 0, Size);

	MmFreeMdl(Mdl);

	return Status;
}

void CcMarkFileModified(PFILE_OBJECT FileObject, uint64_t FileOffset, size_t SizeBytes)
{
	uint64_t SectionOffsetStart = FileOffset / PAGE_SIZE;
	uint64_t SectionOffsetEnd = (FileOffset + SizeBytes - 1) / PAGE_SIZE;

	uint64_t SizePages = SectionOffsetEnd - SectionOffsetStart;

	for (uint64_t i = 0; i < SizePages; i++)
	{
		(void) MmSetPageModifiedMappable(FileObject, SectionOffsetStart + i);
	}
}

// Intended for block devices that have specified a block size through their IO_CACHE_INFO.
//
// This function may fail for an out-of-memory reason, so DO NOT invoke it in the paging
// codepath at all!
bool CcMarkBlockDeviceModified(PFILE_OBJECT FileObject, uint64_t FileOffset, size_t SizeBytes)
{
	size_t BlockSize = FileObject->Fcb->CacheInfo.BlockSize;
	if (BlockSize == 0)
	{
		// No block size specified, there is no reason to continue.
		return true;
	}

	uint64_t EndFileOffset = FileOffset + SizeBytes - 1;

	uint64_t StartBlockOffset = FileOffset / BlockSize;
	uint64_t EndBlockOffset = EndFileOffset / BlockSize;

	return IoSetRangeModifiedCacheInfo(
		&FileObject->Fcb->CacheInfo,
		StartBlockOffset,
		EndBlockOffset - StartBlockOffset + 1
	);
}

// Writes the contents of a buffer to a file through an MDL.
//
// NOTE: This will always block, because we cannot control when a page
// fault will block or will not block.
BSTATUS CcWriteFileMdl(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	PMDL Mdl,
	uintptr_t MdlOffset,
	size_t ByteCount
)
{
	// TODO: Any possible race conditions?!
	//
	// NOTE: Expansion of the file should be handled in IopWriteFile, which calls this.

	BSTATUS Status;
	size_t Size = ByteCount;

	ASSERT(FileObject);

	PFCB Fcb = FileObject->Fcb;
	ASSERT(Fcb);
	CcAcquireMutex(&Fcb->CacheInfo.ViewCacheMutex);

	// Do this before anything else.  The order of operations is important: if we marked the _pages_
	// modified first, they could see the blocks modified bitmap and think "oh, nothing to write!?"
	// and bail before we get to write things.  Also, this can fail, so if we already wrote data but
	// have no way to signal that it was written, we're screwed and will lose data.
	if (!CcMarkBlockDeviceModified(FileObject, FileOffset, ByteCount))
	{
		// The write can't happen because the block device can't have this range of memory modified.
		//
		// However, this only pertains to filesystem metadata, which you don't need access to
		// to perform pageout (unless you plan on expanding the pagefile(s)!?  TODO: think about
		// this carefully.)
		DbgPrint("ERROR: CcWriteFileMdl: Cannot mark block device as modified, out of memory.");
		KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);
		return STATUS_INSUFFICIENT_MEMORY;
	}

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
			KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);
			return Status;
		}

		// The view exists, so let's copy.
		MmCopyFromMdl(Mdl, MdlOffset, (char*)View + ViewOffset, CopyAmount);
		CcMarkFileModified(FileObject, FileOffset, CopyAmount);

		MdlOffset += CopyAmount;
		FileOffset += CopyAmount;
		Size -= CopyAmount;
	}

	KeReleaseMutex(&Fcb->CacheInfo.ViewCacheMutex);
	return STATUS_SUCCESS;
}

// Writes an arbitrary non-MDL buffer to file.
BSTATUS CcWriteFileCopy(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	const void* Buffer,
	size_t Size
)
{
	PMDL Mdl;
	BSTATUS Status;

	Status = MmCreateMdl(&Mdl, (uintptr_t) Buffer, Size, KeGetPreviousMode(), false);
	if (FAILED(Status))
		return Status;

	Status = CcWriteFileMdl(FileObject, FileOffset, Mdl, 0, Size);

	MmFreeMdl(Mdl);

	return Status;
}
