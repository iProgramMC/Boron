/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	io/dir.c
	
Abstract:
	This module implements directory-related system services.
	
Author:
	iProgramInCpp - 4 October 2025
***/
#include "iop.h"

BSTATUS IoResetDirectoryReadHead(PFILE_OBJECT FileObject)
{
	BSTATUS Status;
	PFCB Fcb = FileObject->Fcb;	
	
	Status = IoLockFcbShared(Fcb);
	if (FAILED(Status))
		return Status;
	
	FileObject->DirectoryOffset = 0;
	FileObject->DirectoryVersion = 0;
	
	IoUnlockFcb(Fcb);
	return STATUS_SUCCESS;
}

// NOTE: The DirectoryEntries pointer CAN be a userspace pointer and this is handled
// automatically.
//
// However, DO NOT try to pass a userspace pointer to an Iosb. This case isn't handled.
BSTATUS IoReadDirectoryEntries(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	size_t Count,
	PIO_DIRECTORY_ENTRY DirectoryEntries
)
{
	BSTATUS Status;
	IO_STATUS_BLOCK InternalIosb;
	PFCB Fcb;
	PIO_DISPATCH_TABLE Dispatch;
	IO_DIRECTORY_ENTRY DirEntry;
	uint64_t Offset, Version;
	
	ASSERT(FileObject);
	
	Fcb = FileObject->Fcb;
	ASSERT(Fcb);
	Dispatch = Fcb->DispatchTable;
	ASSERT(Dispatch);
	
	Status = IoLockFcbShared(Fcb);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	Offset  = FileObject->DirectoryOffset;
	Version = FileObject->DirectoryVersion;
	
	IO_READ_DIR_METHOD ReadDir = Dispatch->ReadDir;
	if (ReadDir)
	{
		for (size_t i = 0; i < Count; i++)
		{
			PIO_DIRECTORY_ENTRY PEntry;
			
			if (KeGetPreviousMode() == MODE_KERNEL)
				PEntry = &DirectoryEntries[i];
			else
				PEntry = &DirEntry;
			
			Status = ReadDir(&InternalIosb, FileObject, Offset, Version, PEntry);
			if (IOFAILED(Status))
			{
				Iosb->EntriesRead = i;
				Iosb->Status = InternalIosb.Status;
				goto EarlyFail;
			}
			
			// This function is called by the user-facing OSReadDirectoryEntries as well.
			// To avert the need for an extra memory allocation, simply copy the entry here:
			if (KeGetPreviousMode() != MODE_KERNEL)
			{
				Status = MmSafeCopy(&DirectoryEntries[i], &DirEntry, sizeof(IO_DIRECTORY_ENTRY), KeGetPreviousMode(), true);
				if (FAILED(Status))
				{
					IOSB_STATUS(Iosb, Status);
					goto EarlyFail;
				}
			}
			
			// Move on to the next directory entry.
			Offset  = InternalIosb.ReadDir.NextOffset;
			Version = InternalIosb.ReadDir.Version;
		}
		
		Iosb->EntriesRead = Count;
		Iosb->Status = STATUS_SUCCESS;
	}
	else
	{
		Status = IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	}
	
EarlyFail:
	FileObject->DirectoryOffset  = Offset;
	FileObject->DirectoryVersion = Version;
	
	IoUnlockFcb(Fcb);
	return Status;
}

BSTATUS IoReadDirectoryEntry(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, PIO_DIRECTORY_ENTRY DirectoryEntry)
{
	return IoReadDirectoryEntries(Iosb, FileObject, 1, DirectoryEntry);
}

BSTATUS OSResetDirectoryReadHead(HANDLE FileHandle)
{
	BSTATUS Status;
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(FileHandle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return Status;
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	Status = IoResetDirectoryReadHead(FileObject);
	ObDereferenceObject(FileObject);
	
	return STATUS_SUCCESS;
}

BSTATUS OSReadDirectoryEntries(
	PIO_STATUS_BLOCK Iosb,
	HANDLE FileHandle,
	size_t DirectoryEntryCount,
	PIO_DIRECTORY_ENTRY DirectoryEntries
)
{
	BSTATUS Status;
	
	Status = MmProbeAddress(Iosb, sizeof(*Iosb), true, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	Status = MmProbeAddress(DirectoryEntries, sizeof(IO_DIRECTORY_ENTRY) * DirectoryEntryCount, true, KeGetPreviousMode());
	if (FAILED(Status))
		return Status;
	
	void* FileObjectV;
	Status = ObReferenceObjectByHandle(FileHandle, IoFileType, &FileObjectV);
	if (FAILED(Status))
		return Status;
	
	PFILE_OBJECT FileObject = FileObjectV;
	
	// NOTE: IoReadDirectoryEntries accepts userspace pointers and SPECIFICALLY checks for them.
	// Thus, we do not need a separate memory allocation or anything - just pass the pointer in.
	//
	// However, it does NOT handle Iosb being a userspace address. So we still gotta do this for it.
	IO_STATUS_BLOCK Iosb2;
	Status = IoReadDirectoryEntries(&Iosb2, FileObject, DirectoryEntryCount, DirectoryEntries);
	
	ObDereferenceObject(FileObjectV);
	
	if (FAILED(Status))
		return Status;
	
	Status = MmSafeCopy(Iosb, &Iosb2, sizeof(Iosb), KeGetPreviousMode(), true);
	return Status;
}
