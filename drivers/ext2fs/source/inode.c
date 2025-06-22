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

#define AcquireBlockRwlockShared(in)    ExAcquireSharedRwLock(&(in)->BlockRwlock, false, false, false)
#define AcquireBlockRwlockExclusive(in) ExAcquireExclusiveRwLock(&(in)->BlockRwlock, false, false)
#define DemoteBlockRwlockToShared(in)   ExDemoteToSharedRwLock(&(in)->BlockRwlock)
#define ReleaseBlockRwlock(in) ExReleaseRwLock(&(in)->BlockRwlock)

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
	ExInitializeRwLock(&Ext->BlockRwlock);
	
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

BSTATUS Ext2ReadBlockGroupDescriptor(PEXT2_FILE_SYSTEM FileSystem, uint32_t BlockGroup, PEXT2_BLOCK_GROUP_DESCRIPTOR Descriptor)
{
	// If the block's size is 1024, then the block group descriptor table
	// is at block 2, otherwise it's bigger, so at block 1.
	uint64_t Address = FileSystem->BlockSize == 1024 ? 2048 : 1024;
	
	Address += BlockGroup * sizeof(EXT2_BLOCK_GROUP_DESCRIPTOR);
	
	return CcReadFileCopy(
		FileSystem->File,
		Address,
		Descriptor,
		sizeof *Descriptor
	);
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
	return CcReadFileCopy(
		FileSystem->File,
		Address,
		&Ext->Inode,
		sizeof(EXT2_INODE)
	);
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

// NOTE: The block rwlock must be locked!!
BSTATUS Ext2FindOnDiskBlock(PFCB Fcb, uint32_t BlockIndex, uint32_t* OnDiskBlockOut)
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
		if (Ext->Inode.SinglyIndirectBlockPointer)
			goto ZeroBlock;
		
		uint64_t Address = BLOCK_ADDRESS(Ext->Inode.SinglyIndirectBlockPointer, FileSystem);
		return CcReadFileCopy(FileSystem->File, Address + 4 * BlockIndex, OnDiskBlockOut, sizeof(uint32_t));
	}
	
	BlockIndex -= AddrsPerBlock;
	
	// Is this the doubly indirect block pointer.
	if (BlockIndex < AddrsPerBlock * AddrsPerBlock)
	{
		if (Ext->Inode.DoublyIndirectBlockPointer)
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
		if (Ext->Inode.TriplyIndirectBlockPointer)
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
	*OnDiskBlockOut = 0;
	return STATUS_SUCCESS;
}

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

bool Ext2Seekable(UNUSED PFCB Fcb)
{
	return true;
}

BSTATUS Ext2Read(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	BSTATUS Status;
	size_t Size, MdlOffset;
	size_t BytesRead = 0;
	PEXT2_FILE_SYSTEM FileSystem;
	
	PREP_EXT;
	FileSystem = Ext->OwnerFS;
	
	ASSERT(MdlBuffer->Flags & MDL_FLAG_WRITE);
	
	Size = MdlBuffer->ByteCount;
	MdlOffset = 0;
	
	while (Size)
	{
		// Which block index is this offset within?
		uint32_t BlockIndex = Offset >> FileSystem->BlockSizeLog2;
		uint32_t OnDiskBlock = 0;
		
		AcquireBlockRwlockShared(Ext);
		Status = Ext2FindOnDiskBlock(Fcb, BlockIndex, &OnDiskBlock);
		ReleaseBlockRwlock(Ext);
		if (FAILED(Status))
			break;
		
		uint32_t BlockOffset = Offset & (FileSystem->BlockSize - 1);
		
		size_t BytesTillNext = FileSystem->BlockSize - BlockOffset;
		size_t CopyAmount = Size;
		if (CopyAmount > BytesTillNext)
			CopyAmount = BytesTillNext;
		
		// Okay, now do the read itself.
		if (OnDiskBlock)
		{
			// TODO we don't need to do cached crap please!
			uint64_t Address = BLOCK_ADDRESS(OnDiskBlock, FileSystem) + BlockOffset;
			Status = CcReadFileMdl(FileSystem->File, Address, MdlBuffer, MdlOffset, CopyAmount);
			if (FAILED(Status))
				break;
		}
		else
		{
			MmSetIntoMdl(MdlBuffer, MdlOffset, 0, CopyAmount);
		}
		
		MdlOffset += CopyAmount;
		Offset += CopyAmount;
		BytesRead += CopyAmount;
		Size -= CopyAmount;
	}
	
	Iosb->BytesRead = BytesRead;
	Iosb->Status = Status;
	
	return Status;
}
