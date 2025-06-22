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

#include <string.h>
void DumpHex(void* DataV, size_t DataSize, bool LogScreen)
{
	uint8_t* Data = DataV;
	
	#define A(x) (((x) >= 0x20 && (x) <= 0x7F) ? (x) : '.')
	
	const size_t PrintPerRow = 32;
	
	for (size_t i = 0; i < DataSize; i += PrintPerRow) {
		char Buffer[256];
		Buffer[0] = 0;
		
		//sprintf(Buffer + strlen(Buffer), "%04lx: ", i);
		sprintf(Buffer + strlen(Buffer), "%p: ", Data + i);
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, "   ");
			else
				sprintf(Buffer + strlen(Buffer), "%02x ", Data[i + j]);
		}
		
		strcat(Buffer, "  ");
		
		for (size_t j = 0; j < PrintPerRow; j++) {
			if (i + j >= DataSize)
				strcat(Buffer, " ");
			else
				sprintf(Buffer + strlen(Buffer), "%c", A(Data[i + j]));
		}
		
		if (LogScreen)
			LogMsg("%s", Buffer);
		else
			DbgPrint("%s", Buffer);
	}
}

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
	
	PFILE_OBJECT RootFile;
	Status = IoCreateFileObject(RootFcb, &RootFile, 0, 0);
	if (FAILED(Status))
	{
		DbgPrint("Ext2: Failed to create file object for root dir. %d (%s)", Status, RtlGetStatusString(Status));
		Ext2FreeInode(RootFcb);
	}
	
	// Assign it as the mount root of this device object.
	BackingDevice->MountRoot = RootFile;
	
	// Let's actually print some data about the root inode though.
	PEXT2_INODE I = &EXT(RootFcb)->Inode;
	LogMsg(
		"Info about the root inode:\n"
		"Mode: %u\n"
		"Uid: %u\n"
		"Gid: %u\n"
		"Size: %u\n"
		"AccessTime: %u\n"
		"CreateTime: %u\n"
		"ModifyTime: %u\n"
		"DeleteTime: %u\n"
		"LinkCount: %u\n"
		"BlockCount: %u\n"
		"Flags: %u\n"
		"DirectBlockPointer0: %u\n"
		"DirectBlockPointer1: %u",
		I->Mode,
		I->Uid,
		I->Gid,
		I->Size,
		I->AccessTime,
		I->CreateTime,
		I->ModifyTime,
		I->DeleteTime,
		I->LinkCount,
		I->BlockCount,
		I->Flags,
		I->DirectBlockPointer[0],
		I->DirectBlockPointer[1]
	);
	
	// Now try and read the root inode!
	uint8_t *Data = MmAllocatePool(POOL_NONPAGED, 4096);
	Status = CcReadFileCopy(RootFile, 0, Data, 4096);
	if (FAILED(Status))
		KeCrash("FAILED to read data from root file: %d (%s)", Status, RtlGetStatusString(Status));
	
	LogMsg("Here's the data from the root file.");
	DumpHex(Data, 256, true);
	
	
	MmFreePool(Data);
	
	// Now we can return success, finally.
	LogMsg("Ext2: Found file system on %s", Name);
	
	
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
