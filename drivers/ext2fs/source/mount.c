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
	PEXT2_SUPERBLOCK Sb;
	BSTATUS Status;
	const char* Name = OBJECT_GET_HEADER(BackingDevice)->ObjectName;
	if (!Name)
		Name = "[Unknown]";
	
	// Create the Ext2 file system object.
	Status = Ext2CreateFileSystemObject(&FileSystem);
	if (FAILED(Status))
		return Status;
	
	// Read the super block into it.
	Status = CcReadFileCopy(BackingFile, 1024, &FileSystem->SuperBlock, sizeof(EXT2_SUPERBLOCK));
	if (FAILED(Status))
		goto Failure;
	
	Sb = &FileSystem->SuperBlock;
	
	// Check if the signature is correct.
	if (Sb->Ext2Signature != EXT2_SIGNATURE)
	{
		Status = STATUS_NOT_THIS_FILE_SYSTEM;
		goto Failure;
	}
	
	// Check if we are using the good old revision.
	if (Sb->Version >= 1)
	{
		// No, which means more features are available
		if (Sb->IncompatibleFeatures & EXT2_REQ_UNSUPPORTED_FLAGS)
		{
			LogMsg(
				"Error:   Ext2 partition %s has required features \"%08x\" unsupported by this driver. Cannot continue.",
				Name,
				Sb->IncompatibleFeatures & EXT2_REQ_UNSUPPORTED_FLAGS
			);
			Status = STATUS_UNIMPLEMENTED;
			goto Failure;
		}
		
		if (Sb->ReadOnlyFeatures & EXT2_ROF_UNSUPPORTED_FLAGS)
		{
			LogMsg(
				"Warning: Ext2 partition %s has read-write-required features \"%08x\" unsupported by this driver. Volume will be read-only.",
				Name,
				Sb->ReadOnlyFeatures & EXT2_ROF_UNSUPPORTED_FLAGS
			);
			FileSystem->ForceReadOnly = true;
		}
		
		if (Sb->CompatibleFeatures & EXT2_OPT_UNSUPPORTED_FLAGS)
		{
			LogMsg(
				"Warning: Ext2 partition %s has optional features \"%08x\" unsupported by this driver.",
				Name,
				Sb->ReadOnlyFeatures & EXT2_OPT_UNSUPPORTED_FLAGS
			);
		}
	}
	
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
	// TODO
}
