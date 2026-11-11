/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	inode.c

Abstract:
	This module manages Ext2 inode instances through FCBs.

Author:
	iProgramInCpp - 21 June 2025
***/
#include "ext2.h"
#include "inode.h"

int Ext2TypeIndicatorToFileType(uint8_t TypeIndicator)
{
	switch (TypeIndicator)
	{
		case EXT2_FT_REG_FILE:
			return FILE_TYPE_FILE;
		case EXT2_FT_DIR:
			return FILE_TYPE_DIRECTORY;
		case EXT2_FT_SYMLINK:
			return FILE_TYPE_SYMBOLIC_LINK;
		case EXT2_FT_CHRDEV:
			return FILE_TYPE_CHARACTER_DEVICE;
		case EXT2_FT_BLKDEV:
			return FILE_TYPE_BLOCK_DEVICE;
		case EXT2_FT_FIFO:
			return FILE_TYPE_PIPE;
		case EXT2_FT_SOCK:
			return FILE_TYPE_SOCKET;
		default:
			return FILE_TYPE_UNKNOWN;
	}
}

int Ext2InodeModeToFileType(uint16_t Mode)
{
	switch (Mode & EXT2_S_IFMSK)
	{
		case EXT2_S_IFREG:
			return FILE_TYPE_FILE;
		case EXT2_S_IFDIR:
			return FILE_TYPE_DIRECTORY;
		case EXT2_S_IFLNK:
			return FILE_TYPE_SYMBOLIC_LINK;
		case EXT2_S_IFCHR:
			return FILE_TYPE_CHARACTER_DEVICE;
		case EXT2_S_IFBLK:
			return FILE_TYPE_BLOCK_DEVICE;
		case EXT2_S_IFIFO:
			return FILE_TYPE_PIPE;
		case EXT2_S_IFSOCK:
			return FILE_TYPE_SOCKET;
		default:
			return FILE_TYPE_UNKNOWN;
	}
}

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
	Ext->InodeDataIsDirty = false;
	ObReferenceObjectByPointer(FileSystem);
	ExInitializeRwLock(&Ext->BlockRwlock);
	ExInitializeFastMutex(&Ext->InodeDataMutex);

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

	ExDeinitializeRwLock(&Ext->BlockRwlock);

	IoFreeFcb(Fcb);
}

void Ext2ReferenceInode(PFCB Fcb)
{
	PREP_EXT;
	AtAddFetch(Ext->ReferenceCount, 1);
}

void Ext2CreateObject(PFCB Fcb, UNUSED PFILE_OBJECT FileObject)
{
	Ext2ReferenceInode(Fcb);
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
	EXT2_BLOCK_GROUP_DESCRIPTOR Descriptor;
	BSTATUS Status;
	PREP_EXT;

	if (Ext->InodeNumber == 0)
		return STATUS_INVALID_PARAMETER;

	uint32_t InodeNo = Ext->InodeNumber - 1;

	// Determine which block group this inode belongs to.
	uint32_t InodesPerGroup = FileSystem->SuperBlock.InodesPerGroup;
	uint32_t BlockGroup = InodeNo / InodesPerGroup;

	Status = Ext2ReadBlockGroupDescriptor(FileSystem, BlockGroup, &Descriptor);
	if (FAILED(Status))
		return Status;

	// Get the block group's inode table address and the
	// inode's index within that block group table.
	uint32_t InodeTableAddr  = Descriptor.InodeTableBlockId;
	uint32_t InodeTableIndex = InodeNo % InodesPerGroup;

	// And finally, read.
	uint64_t Address = BLOCK_ADDRESS(InodeTableAddr, FileSystem) + InodeTableIndex * FileSystem->InodeSize;
	Status = CcReadFileCopy(
		FileSystem->File,
		Address,
		&Ext->Inode,
		sizeof(EXT2_INODE)
	);
	if (FAILED(Status))
		return Status;

	// Now, fill in this FCB's data:
	Fcb->FileType = Ext2InodeModeToFileType(Ext->Inode.Mode);
	Fcb->FileLength = Ext2FileSize(Fcb);
	Ext->InodeWritebackAddress = Address;
	return STATUS_SUCCESS;
}

BSTATUS Ext2OpenInode(PEXT2_FILE_SYSTEM FileSystem, uint32_t InodeNumber, PFCB* OutFcb)
{
	// Check if it exists in the file system at all.
	if (InodeNumber >= FileSystem->SuperBlock.InodeCount)
		return STATUS_INVALID_PARAMETER;

	PRBTREE_ENTRY Entry = NULL;
	PFCB Fcb = NULL;
	BSTATUS Status = STATUS_INVALID_PARAMETER;

	// Check if it exists within the tree.
	AcquireInodeTreeMutex(FileSystem);

Retry:
	Entry = LookUpItemRbTree(&FileSystem->InodeTree, InodeNumber);
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

//
// Finds an on-disk block number, by its offset within the file, and returns it.
//
// NOTE: The caller must lock the block rwlock of the inode:
// - shared, if AllocateIfNeeded is false
// - exclusive, if AllocateIfNeeded is true
//
BSTATUS Ext2pFindOnDiskBlock(PFCB Fcb, uint32_t BlockIndex, uint32_t* OnDiskBlockOut, bool AllocateIfNeeded)
{
	BSTATUS Status;
	PREP_EXT;
	PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;

	uintptr_t AddrsPerBlock = FileSystem->BlockSize >> 2;

	if (BlockIndex < 12)
	{
		// Oh, easy, this is just the index within the block.
		*OnDiskBlockOut = Ext->Inode.DirectBlockPointer[BlockIndex];
		return STATUS_SUCCESS;
	}

	BlockIndex -= 12;

	// Is this the singly indirect block pointer.
	if (BlockIndex < AddrsPerBlock)
	{
		if (!Ext->Inode.SinglyIndirectBlockPointer)
			goto ZeroBlock;

		uint64_t Address = BLOCK_ADDRESS(Ext->Inode.SinglyIndirectBlockPointer, FileSystem);
		return CcReadFileCopy(FileSystem->File, Address + 4 * BlockIndex, OnDiskBlockOut, sizeof(uint32_t));
	}

	BlockIndex -= AddrsPerBlock;

	// Is this the doubly indirect block pointer.
	if (BlockIndex < AddrsPerBlock * AddrsPerBlock)
	{
		if (!Ext->Inode.DoublyIndirectBlockPointer)
			goto ZeroBlock;

		// TODO: Optimize these divisions away
		uint32_t Part1 = BlockIndex / AddrsPerBlock;
		uint32_t Part2 = BlockIndex % AddrsPerBlock;
		uint64_t Address;

		Address = BLOCK_ADDRESS(Ext->Inode.DoublyIndirectBlockPointer, FileSystem);
		Status = CcReadFileCopy(FileSystem->File, Address + 4 * Part1, &Part1, sizeof(uint32_t));
		if (FAILED(Status))
			return Status;

		if (Part1 == 0)
			goto ZeroBlock;

		Address = BLOCK_ADDRESS(Part1, FileSystem);
		return CcReadFileCopy(FileSystem->File, Address + 4 * Part2, OnDiskBlockOut, sizeof(uint32_t));
	}

	BlockIndex -= AddrsPerBlock;

	if (BlockIndex < AddrsPerBlock * AddrsPerBlock * AddrsPerBlock)
	{
		if (!Ext->Inode.TriplyIndirectBlockPointer)
			goto ZeroBlock;

		// TODO: Optimize these divisions away
		uint32_t Part1 = BlockIndex / AddrsPerBlock / AddrsPerBlock;
		uint32_t Part2 = BlockIndex / AddrsPerBlock % AddrsPerBlock;
		uint32_t Part3 = BlockIndex % AddrsPerBlock;
		uint64_t Address;

		Address = BLOCK_ADDRESS(Ext->Inode.TriplyIndirectBlockPointer, FileSystem);
		Status = CcReadFileCopy(FileSystem->File, Address + 4 * Part1, &Part1, sizeof(uint32_t));
		if (FAILED(Status))
			return Status;

		if (Part1 == 0)
			goto ZeroBlock;

		Address = BLOCK_ADDRESS(Part1, FileSystem);
		Status = CcReadFileCopy(FileSystem->File, Address + 4 * Part2, &Part2, sizeof(uint32_t));
		if (FAILED(Status))
			return Status;

		if (Part2 == 0)
			goto ZeroBlock;

		Address = BLOCK_ADDRESS(Part2, FileSystem);
		return CcReadFileCopy(FileSystem->File, Address + 4 * Part3, OnDiskBlockOut, sizeof(uint32_t));
	}

	// Can't write more data
	return STATUS_INSUFFICIENT_SPACE;

ZeroBlock:
	if (AllocateIfNeeded)
	{
		// TODO: Unimplemented
		DbgPrint("NYI Ext2pFindOnDiskBlock, with allocation");
		return STATUS_UNIMPLEMENTED;
	}

	*OnDiskBlockOut = 0;
	return STATUS_SUCCESS;
}

bool Ext2Seekable(UNUSED PFCB Fcb)
{
	return true;
}

static BSTATUS Ext2pFlushSuperBlock(PEXT2_FILE_SYSTEM FileSystem)
{
	BSTATUS Status;
	EXT2_SUPERBLOCK SuperBlock;

	if (!Ext2GetSuperBlockSnapshot(FileSystem, &SuperBlock, true, false))
	{
		// Clean superblock.
		return STATUS_SUCCESS;
	}

	Status = CcWriteFileCopy(
		FileSystem->File,
		1024,
		&SuperBlock,
		sizeof(EXT2_SUPERBLOCK)
	);

	if (FAILED(Status))
	{
		FileSystem->SuperBlockIsDirty = true;
	}

	return Status;
}

BSTATUS Ext2FlushMetadata(PFCB Fcb)
{
	BSTATUS Status = STATUS_SUCCESS;
	PREP_EXT;
	PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;

	EXT2_INODE InodeCopy;
	bool WritebackInodeCopy = false;

	if (FileSystem->ForceReadOnly)
	{
		return STATUS_SUCCESS;
	}

	// If this node's metadata is dirty, write it.
	ExAcquireFastMutex(&Ext->InodeDataMutex, false);

	if (Ext->InodeDataIsDirty)
	{
		memcpy(&InodeCopy, &Ext->Inode, sizeof(EXT2_INODE));
		Ext->InodeDataIsDirty = false;
		WritebackInodeCopy = true;
	}

	ExReleaseFastMutex(&Ext->InodeDataMutex);

	if (WritebackInodeCopy)
	{
		Status = CcWriteFileCopy(
			FileSystem->File,
			Ext->InodeWritebackAddress,
			&InodeCopy,
			sizeof(EXT2_INODE)
		);

		if (FAILED(Status))
		{
			DbgPrint("Ext2: Cannot flush inode metadata to disk: %s", RtlGetStatusString(Status));
			return Status;
		}
	}

	// TODO: Flush the block mapping data to disk here? Too complicated for now,
	// so we'll hand it off to the disk cache instead.

	if (Fcb != FileSystem->RootInode)
		return Status;

	// We're the root inode.  There are some special semantics: a call to flush
	// the whole node leads to a complete synchronization for this file system.
	AcquireInodeTreeMutex(FileSystem);

	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&FileSystem->InodeTree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PFCB ChildFcb = FCB(CONTAINING_RECORD(Entry, EXT2_FCB_EXT, InodeTreeEntry));
		if (ChildFcb == Fcb)
			continue;

		// NOTE: Ignoring status.
		(void) MmFlushFileCache(ChildFcb);
		(void) Ext2FlushMetadata(ChildFcb);
	}

	ReleaseInodeTreeMutex(FileSystem);

	Status = Ext2pFlushSuperBlock(FileSystem);
	if (FAILED(Status))
	{
		DbgPrint("Ext2: Could not flush super block: %s", RtlGetStatusString(Status));
		return Status;
	}

	// Call flush on the backing disk, too.
	return MmFlushFileCache(FileSystem->File->Fcb);
}

BSTATUS Ext2TouchFile(PFCB Fcb, bool IsWrite)
{
	// TODO
	DbgPrint("TODO: Ext2TouchFile(%p, %s)", Fcb, IsWrite ? "WRITE" : "READ");
	return STATUS_SUCCESS;
}
