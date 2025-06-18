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
OBJECT_TYPE_INFO Ext2FileSystemTypeInfo =
{
	.Delete = Ext2DeleteFileSystem,
};

POBJECT_TYPE Ext2FileSystemType;

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
