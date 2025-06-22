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

BSTATUS Ext2Mount(PDEVICE_OBJECT BackingDevice, PFILE_OBJECT BackingFile, POBJECT_DIRECTORY MountDir)
{
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
		
		// Fill in some details which are not hardcoded in EXT2_DYNAMIC_REV.
		FileSystem->FirstInode = Sb->FirstNonReservedInode;
		FileSystem->InodeSize = Sb->InodeStructureSize;
	}
	else
	{
		// These details are hardecoded in EXT2_GOOD_OLD_REV
		FileSystem->FirstInode = EXT2_DEF_FIRST_INODE;
		FileSystem->InodeSize = EXT2_DEF_INODE_SIZE;
	}
	
	FileSystem->BlockSizeLog2 = 10 + Sb->BlockSizeLog2;
	FileSystem->BlockSize    = 1024 << Sb->BlockSizeLog2;
	FileSystem->FragmentSize = 1024 << Sb->FragmentSizeLog2;
	FileSystem->InodesPerGroup = Sb->InodesPerGroup;
	FileSystem->BlocksPerGroup = Sb->BlocksPerGroup;
	
	// Initialize the rest of the file system data structure.
	KeInitializeMutex(&FileSystem->InodeTreeMutex, 0);
	InitializeRbTree(&FileSystem->InodeTree);
	
	FileSystem->BackingDevice = BackingDevice;
	FileSystem->File = BackingFile;
	
	// Read the "/" inode.  It is inode 2.
	PFCB RootFcb;
	Status = Ext2OpenInode(FileSystem, 2, &RootFcb);
	if (FAILED(Status))
	{
		DbgPrint("Ext2: Failed to read root inode. %d (%s)", Status, RtlGetStatusString(Status));
		goto Failure;
	}
	
	FileSystem->RootInode = RootFcb;
	
	// Now, we can link the object into the mount directory, hopefully.
	Status = ObLinkObject(MountDir, FileSystem, Name);
	if (FAILED(Status))
	{
		DbgPrint("Ext2: Failed to link to mount dir. %d (%s)", Status, RtlGetStatusString(Status));
		goto Failure;
	}
	
	// Now we can return success, finally.
	LogMsg("Ext2: Found file system on %s.", Name);
	
	
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

BSTATUS Ext2ParseFileSystem(
	void* FileSystemV,
	UNUSED const char** Name,
	UNUSED void* Context,
	UNUSED int LoopCount,
	void** OutObject
)
{
	PEXT2_FILE_SYSTEM FileSystem = FileSystemV;
	DbgPrint("Ext2ParseFileSystem");
	
	return IoCreateFileObject(FileSystem->RootInode, (PFILE_OBJECT*) OutObject, 0, 0);
}
