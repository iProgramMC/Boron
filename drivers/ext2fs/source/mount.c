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

	FileSystem->BackingDevice = BackingDevice;
	FileSystem->File = BackingFile;

	IoSetBlockSizeFile(BackingFile, FileSystem->BlockSize);

	// Allocate a page for the temporary page-size read buffer.
	MMPFN Pfn = MmAllocatePhysicalPage();
	if (Pfn == PFN_INVALID)
	{
		DbgPrint("Ext2: Failed to allocate temporary page-size read buffer");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	// Then map it into memory.
	FileSystem->TempPageSizeBuffer = MmMapIoSpace(
		MmPFNToPhysPage(Pfn),
		PAGE_SIZE,
		MM_PROT_READ | MM_PROT_WRITE,
		POOL_TAG("E2Tb")
	);

	if (!FileSystem->TempPageSizeBuffer)
	{
		MmFreePhysicalPage(Pfn);
		DbgPrint("Ext2: Failed to allocate temporary page-size read buffer");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	MmInitializeSinglePageMdl(
		&FileSystem->TempPageSizeBufferMdl,
		Pfn,
		MDL_FLAG_WRITE
	);

	// Allocate a block for the temporary block-size read buffer.
	void* BlockSizeBuffer = MmAllocatePool(POOL_NONPAGED, FileSystem->BlockSize);
	if (!BlockSizeBuffer)
	{
		DbgPrint("Ext2: Failed to allocate temporary block-size read buffer");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	Status = MmCreateMdl(
		&FileSystem->TempBlockSizeBufferMdl,
		(uintptr_t) BlockSizeBuffer,
		FileSystem->BlockSize,
		MODE_KERNEL,
		true
	);
	if (FAILED(Status))
	{
		MmFreePool(BlockSizeBuffer);
		DbgPrint("Ext2: Failed to allocate MDL for temporary block-size read buffer");
		goto Failure;
	}

	FileSystem->TempBlockSizeBuffer = BlockSizeBuffer;

	// Finally, allocate a completely zeroed page for certain cleaning operations.
	MMPFN ZeroPagePfn = MmAllocatePhysicalPage();
	if (ZeroPagePfn == PFN_INVALID)
	{
		DbgPrint("Ext2: Failed to allocate temporary zero page buffer");
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Failure;
	}

	MmInitializeSinglePageMdl(
		&FileSystem->ZeroPageBufferMdl,
		ZeroPagePfn,
		0 // Flags
	);

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

	return IoCreateFileObject(FileSystem->RootInode, (PFILE_OBJECT*) OutObject, 0);
}
