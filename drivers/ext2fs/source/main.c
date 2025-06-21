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
	.Mount = Ext2Mount,
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
};

POBJECT_TYPE Ext2FileSystemType;

BSTATUS Ext2CreateFileSystemObject(PEXT2_FILE_SYSTEM* Out)
{
	void* Object = NULL;
	BSTATUS Status = ObCreateObject(
		&Object,
		NULL,
		Ext2FileSystemType,
		NULL,
		OB_FLAG_KERNEL | OB_FLAG_NO_DIRECTORY,
		NULL,
		sizeof(EXT2_FILE_SYSTEM)
	);
	
	if (FAILED(Status))
	{
		LogMsg("Ext2: failed to create file system object: %d (%s)", Status, RtlGetStatusString(Status));
		return Status;
	}
	
	*Out = Object;
	return STATUS_SUCCESS;
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
