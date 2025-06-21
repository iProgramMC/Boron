/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mount.c
	
Abstract:
	This module takes care of the process of mounting
	an Ext2 partition.
	
Author:
	iProgramInCpp - 15 May 2025
***/
#include "ext2.h"

BSTATUS Ext2Mount(PDEVICE_OBJECT BackingDevice, PFILE_OBJECT BackingFile)
{
	LogMsg("Sup");
	PEXT2_FILE_SYSTEM FileSystem;
	BSTATUS Status;
	
	// Create the Ext2 file system object.
	Status = Ext2CreateFileSystemObject(&FileSystem);
	if (FAILED(Status)) {
		DbgPrint("ERROR: %d (%s) at mount.c:%d", Status, RtlGetStatusString(Status), __LINE__);
		return Status;
	}
	
	// Read the super block into it.
	Status = CcReadFileCopy(BackingFile, 1024, &FileSystem->SuperBlock, sizeof(EXT2_SUPERBLOCK));
	if (FAILED(Status)) {
		DbgPrint("ERROR: %d (%s) at mount.c:%d", Status, RtlGetStatusString(Status), __LINE__);
		goto Failure;
	}
	
	// Check if the signature is correct.
	if (FileSystem->SuperBlock.Ext2Signature != EXT2_SIGNATURE)
	{
		Status = STATUS_NOT_THIS_FILE_SYSTEM;
		DbgPrint("ERROR: %d (%s) at mount.c:%d   Sig: %04x", Status, RtlGetStatusString(Status), __LINE__, FileSystem->SuperBlock.Ext2Signature);
		goto Failure;
	}
	
	// Check if there are any required features.
	
	
	LogMsg("Ext2: Found a file system!");
	return STATUS_UNIMPLEMENTED;
	
Failure:
	ObDereferenceObject(FileSystem);
	return Status;
}

void Ext2DeleteFileSystem(void* FileSystemV)
{
	PEXT2_FILE_SYSTEM FileSystem = FileSystemV;
	
	DbgPrint("Ext2: File System %p Deleted", FileSystem);
	KeCrash("Unimplemented");
}
