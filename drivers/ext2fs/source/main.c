/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	main.c

Abstract:
	This module implements the main function of the
	Ext2 File System driver.

Author:
	iProgramInCpp - 30 April 2025
***/
#include "ext2.h"

IO_DISPATCH_TABLE Ext2DispatchTable =
{
	.CreateObject = Ext2CreateObject,
	.Reference = Ext2ReferenceInode,
	.Dereference = Ext2DereferenceInode,
	.Mount = Ext2Mount,
	.Read = Ext2Read,
	.ReadDir = Ext2ReadDir,
	.ParseDir = Ext2ParseDir,
	.Seekable = Ext2Seekable,
	.Resize = Ext2Resize,
	.FlushMetadata = Ext2FlushMetadata,
};

// The only reason we make a type of object is so that we
// can track the number of references to the Ext2 file system
// instance using the object manager's reference counting
// mechanisms.
//
// This object is not exposed in the object namespace because
// we don't need to do that. What we *do* expose is the
// actual device object that acts as a mount point.
OBJECT_TYPE_INFO Ext2FileSystemTypeInfo =
{
	.Delete = Ext2DeleteFileSystem,
	.Parse = Ext2ParseFileSystem,
};

POBJECT_TYPE Ext2FileSystemType;

BSTATUS Ext2CreateFileSystemObject(PEXT2_FILE_SYSTEM* Out)
{
	void* Object = NULL;

	// The object will be assigned a directory later.
	BSTATUS Status = ObCreateObject(
		&Object,
		NULL,
		Ext2FileSystemType,
		NULL,
		OB_FLAG_NO_DIRECTORY,
		sizeof(EXT2_FILE_SYSTEM)
	);

	if (FAILED(Status))
	{
		LogMsg("Ext2: failed to create file system object: %d (%s)", Status, RtlGetStatusString(Status));
		return Status;
	}

	PEXT2_FILE_SYSTEM FileSystem = Object;

	ExInitializeFastMutex(&FileSystem->InodeTreeMutex);
	InitializeRbTree(&FileSystem->InodeTree);

	ExInitializeFastMutex(&FileSystem->BlockAllocationMutex);
	ExInitializeFastMutex(&FileSystem->InodeAllocationMutex);
	ExInitializeFastMutex(&FileSystem->TempPageSizeBufferMutex);
	ExInitializeFastMutex(&FileSystem->TempBlockSizeBufferMutex);

	KeInitializeSpinLock(&FileSystem->SuperBlockSpinLock);

	*Out = FileSystem;
	return STATUS_SUCCESS;
}

void Ext2DeleteFileSystem(void* FileSystemV)
{
	PEXT2_FILE_SYSTEM FileSystem = FileSystemV;
	DbgPrint("Ext2: File System %p Deleted (TODO: clean up)", FileSystem);
	// TODO
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	BSTATUS Status;

	// Declare the "Ext2FileSystem" type.
	Status = ObCreateObjectType("Ext2FileSystem", &Ext2FileSystemTypeInfo, &Ext2FileSystemType);
	if (FAILED(Status))
		KeCrash("Failed to create Ext2FileSystem type: %d (%s)", Status, RtlGetStatusString(Status));

	// Now register this file system as a valid FSD.
	IoRegisterFileSystemDriver(&Ext2DispatchTable);
	return STATUS_SUCCESS;
}
