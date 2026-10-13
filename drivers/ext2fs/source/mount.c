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

	DbgPrint("Ext2Mount: Mounting %s...", Name);

	// Create the Ext2 file system object.
	Status = Ext2CreateFileSystemObject(&FileSystem);
	if (FAILED(Status))
		return Status;

	FileSystem->BackingDevice = BackingDevice;
	FileSystem->File = BackingFile;

	// Create the virtual metadata file for the superblock.
	FileSystem->SuperBlockFile = Ext2CreateMetadataFile(FileSystem, MD_SUPERBLOCK);

	if (!FileSystem->SuperBlockFile)
	{
		DbgPrint("Ext2: Could not allocate the super block virtual file.  Can't continue.");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	FileSystem->BlockSize = 1024;

	// Read the super block into it.
	//
	// NOTE: When writing, DO NOT modify the super block through the partition's cache.
	// TODO: Add a way to disable reading from a partition via its page cache.
	Status = CcReadFileCopy(
		FileSystem->SuperBlockFile,
		0,                            // Offset
		&FileSystem->SuperBlock,
		sizeof(EXT2_SUPERBLOCK)
	);
	if (FAILED(Status))
		goto Failure;

	Sb = &FileSystem->SuperBlock;

	// Check if the signature is correct.
	if (Sb->Ext2Signature != EXT2_SIGNATURE)
	{
		Status = STATUS_NOT_THIS_FILE_SYSTEM;
		goto Failure;
	}

	// Check if we are using the "good old" (0) revision.
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
		// These details are hardcoded in EXT2_GOOD_OLD_REV
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

	// Create the rest of the metadata files, which depend on the superblock.
	// clang-format off
	FileSystem->SuperBlockBackupsFile = Ext2CreateMetadataFile(FileSystem, MD_SUPERBLOCK_BACKUPS);
	FileSystem->BlockGroupDescriptorFile = Ext2CreateMetadataFile(FileSystem, MD_BLOCK_GROUP_DESCRIPTORS);
	FileSystem->BlockGroupDescriptorBackupsFile = Ext2CreateMetadataFile(FileSystem, MD_BLOCK_GROUP_DESCRIPTOR_BACKUPS);
	FileSystem->BlockBitmapFile = Ext2CreateMetadataFile(FileSystem, MD_BLOCK_BITMAPS);
	FileSystem->InodeBitmapFile = Ext2CreateMetadataFile(FileSystem, MD_INODE_BITMAPS);
	FileSystem->InodeTableFile = Ext2CreateMetadataFile(FileSystem, MD_INODE_TABLES);
	// clang-format off

	if (!FileSystem->SuperBlockBackupsFile ||
		!FileSystem->BlockGroupDescriptorFile ||
		!FileSystem->BlockGroupDescriptorBackupsFile ||
		!FileSystem->BlockBitmapFile ||
		!FileSystem->InodeBitmapFile ||
		!FileSystem->InodeTableFile)
	{
		DbgPrint("Ext2: Could not allocate one or more virtual file objects.  Can't continue.");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}


	// Read the "/" inode.  It is inode 2.
	PFCB RootFcb;
	Status = Ext2OpenInode(FileSystem, 2, &RootFcb);
	if (FAILED(Status))
	{
		DbgPrint("Ext2: Failed to read root inode. %d (%s)", Status, RtlGetStatusString(Status));
		goto Failure;
	}

	if (RootFcb->FileType != FILE_TYPE_DIRECTORY) {
		DbgPrint("WARNING: Root inode is not a directory.  Things'll go wrong quick!");
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
	DbgPrint("Ext2: File System %p Deleted (TODO: clean up)", FileSystem);
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
