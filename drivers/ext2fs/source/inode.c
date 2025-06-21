/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mount.c
	
Abstract:
	This module manages Ext2 inode instances through FCBs.
	
Author:
	iProgramInCpp - 21 June 2025
***/
#include "ext2.h"

// Prepare the extension.
#define PREP_EXT PEXT2_FCB_EXT Ext = EXT(Fcb)

#define AcquireInodeTreeMutex(fs) KeWaitForSingleObject(&(fs)->InodeTreeMutex, false, TIMEOUT_INFINITE, MODE_KERNEL)
#define ReleaseInodeTreeMutex(fs) KeReleaseMutex(&(fs)->InodeTreeMutex)

PFCB Ext2CreateFcb(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber)
{
	PFCB Fcb = IoAllocateFcb(&Ext2DispatchTable, sizeof(EXT2_FCB_EXT), false);
	if (!Fcb)
		return Fcb;
	
	PREP_EXT;
	Ext->InodeNumber = InodeNumber;
	Ext->ReferenceCount = 1;
	Ext->OwnerFS = FileSystem;
	Ext->InodeTreeEntry.Key = InodeNumber;
	ObReferenceObjectByPointer(FileSystem);
	
	return Fcb;
}

void Ext2FreeInode(PFCB Fcb)
{
	DbgPrint("Ext2FreeInode: %p", Fcb);
	
	PREP_EXT;
	
	if (Ext->OwnerFS)
	{
		PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;
		Ext->OwnerFS = NULL;
		
		// Remove it from the inode tree if it was added.
		if (Ext->AddedToInodeTree)
		{
			Ext->AddedToInodeTree = false;
			AcquireInodeTreeMutex(FileSystem);
			RemoveItemRbTree(&FileSystem->InodeTree, &Ext->InodeTreeEntry);
			ReleaseInodeTreeMutex(FileSystem);
		}
		
		// Dereference the file system object too.
		ObDereferenceObject(FileSystem);
	}
	
	IoFreeFcb(Fcb);
}

void Ext2ReferenceInode(PFCB Fcb)
{
	PREP_EXT;
	
	uintptr_t Value = AtAddFetch(Ext->ReferenceCount, 1);
	if (Value == 1)
	{
		// TODO: Freshly referenced, remove it off of the to-be-reclaimed list.
	}
}

void Ext2DereferenceInode(PFCB Fcb)
{
	PREP_EXT;
	
	uintptr_t Value = AtAddFetch(Ext->ReferenceCount, -1);
	if (Value == 0)
	{
		// TODO: Freshly dereferenced, add it to the to-be-reclaimed list.
		Ext2FreeInode(Fcb);
	}
}

BSTATUS Ext2ReadInode(PEXT2_FILE_SYSTEM FileSystem, PFCB Fcb)
{
	PREP_EXT;
	
	// Determine which block group this inode belongs to.
	// TODO
	return STATUS_UNIMPLEMENTED;
}

BSTATUS Ext2OpenInode(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber, PFCB* OutFcb)
{
	// Check if it exists in the file system at all.
	if (InodeNumber >= FileSystem->SuperBlock.InodeCount)
		return STATUS_INVALID_PARAMETER;
	
	PFCB Fcb = NULL;
	BSTATUS Status = STATUS_INVALID_PARAMETER;
	
	// Check if it exists within the tree.
	AcquireInodeTreeMutex(FileSystem);
	
Retry:
	PRBTREE_ENTRY Entry = LookUpItemRbTree(&FileSystem->InodeTree, InodeNumber);
	if (Entry)
	{
		// Get the FCB and reference it.
		Fcb = FCB(CONTAINING_RECORD(Entry, EXT2_FCB_EXT, InodeTreeEntry));
		Ext2ReferenceInode(Fcb);
	}
	
	ReleaseInodeTreeMutex(FileSystem);
	
	if (Fcb)
	{
		*OutFcb = Fcb;
		return STATUS_SUCCESS;
	}
	
	// The FCB doesn't exist.  Therefore we need to load it in.
	Fcb = Ext2CreateFcb(FileSystem, InodeNumber);
	if (!Fcb)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Now read.
	Status = Ext2ReadInode(FileSystem, Fcb);
	if (FAILED(Status))
	{
		Ext2DereferenceInode(Fcb);
		return Status;
	}
	
	// The inode has been read, let's add it to the tree.
	AcquireInodeTreeMutex(FileSystem);
	
	bool Inserted = InsertItemRbTree(&FileSystem->InodeTree, &EXT(Fcb)->InodeTreeEntry);
	
	if (!Inserted)
	{
		// Then chances are it's already loaded.
		Ext2FreeInode(Fcb);
		Fcb = NULL;
		goto Retry;
	}
	
	ReleaseInodeTreeMutex(FileSystem);
	
	*OutFcb = Fcb;
	return STATUS_SUCCESS;
}
