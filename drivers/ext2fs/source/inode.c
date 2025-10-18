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

uint64_t Ext2FileSize(PFCB Fcb)
{
	PREP_EXT;
	
	if (Ext->OwnerFS->SuperBlock.Version == 0)
		return Ext->Inode.Size;
	else
		return Ext->Inode.Size | ((uint64_t) Ext->Inode.SizeUpper << 32);
}

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
	
	uint64_t FileSize = Ext2FileSize(Fcb);
	
	// Check for an overflow.
	if (Size + Offset < Size || Size + Offset < Offset)
		return IOSB_STATUS(Iosb, STATUS_INVALID_PARAMETER);
	
	// Ensure a cap over the size of the file.
	if (Offset >= FileSize) {
		Iosb->BytesRead = 0;
		return IOSB_STATUS(Iosb, STATUS_SUCCESS);
	}
	
	if (Size + Offset >= FileSize)
		Size = FileSize - Offset;
	
	// TODO: A better way?
	uint8_t* BlockBuffer = MmAllocatePool(POOL_NONPAGED, FileSystem->BlockSize);
	if (!BlockBuffer)
		// TODO: Use the builtin one for paging if Flags & PAGING
		return STATUS_INSUFFICIENT_MEMORY;
	
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
			uint64_t Address = BLOCK_ADDRESS(OnDiskBlock, FileSystem);
			Status = IoReadFile(Iosb, FileSystem->File, BlockBuffer, FileSystem->BlockSize, Address, false);
			if (FAILED(Status))
				break;
			
			MmCopyIntoMdl(MdlBuffer, MdlOffset, BlockBuffer + BlockOffset, CopyAmount);
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
	
	MmFreePool(BlockBuffer);
	return Status;
}

BSTATUS Ext2ReadCopy(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, void* Buffer, size_t Size, uint32_t Flags)
{
	PMDL Mdl;
	BSTATUS Status;
	
	Status = MmCreateMdl(&Mdl, (uintptr_t) Buffer, Size, KeGetPreviousMode(), true);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	Status = Ext2Read(Iosb, Fcb, Offset, Mdl, Flags);
	
	MmFreeMdl(Mdl);
	
	return Status;
}

BSTATUS Ext2ReadDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, uint64_t Offset, uint64_t Version, PIO_DIRECTORY_ENTRY DirectoryEntry)
{
	BSTATUS Status;
	PFCB Fcb = FileObject->Fcb;
	PREP_EXT;
	PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;
	
	// TODO: Check if this file was modified by comparing the Version
	// with the FCB's version.  If it was, then we need to re-validate
	// the current offset.
	//
	// Linux reads directory entries until the current offset is overtaken, probably
	// to avoid looping situations.  We should do the same.
	(void) Version;
	
	EXT2_DIRENT Dirent;
	uint64_t NewVersion = Ext->Version;
	
	if (Offset >= Ext2FileSize(Fcb))
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);
	
	// Since we can only do reads at the moment, just read.
	Status = IoReadFile(Iosb, FileObject, &Dirent, sizeof(EXT2_DIRENT), Offset, true);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	if (Iosb->BytesRead < sizeof(EXT2_DIRENT))
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);
	
	// Read the name, and ensure fits within our IO_NAME_MAX-1 characters.
	size_t NameLength = Dirent.NameLength;
	if (FileSystem->SuperBlock.Version == 0)
		NameLength = Dirent.NameLength16;
	
	if (NameLength > IO_MAX_NAME - 1)
		NameLength = IO_MAX_NAME - 1;
	
	Status = IoReadFile(Iosb, FileObject, &DirectoryEntry->Name, NameLength, Offset + sizeof(EXT2_DIRENT), true);
	if (FAILED(Status))
		return IOSB_STATUS(Iosb, Status);
	
	if (Iosb->BytesRead < NameLength)
		return IOSB_STATUS(Iosb, STATUS_END_OF_FILE);
	
	DirectoryEntry->Name[NameLength] = 0;
	
	// Also fill in the inode # and type.
	DirectoryEntry->InodeNumber = Dirent.InodeNumber;
	
	if (FileSystem->SuperBlock.Version == 0)
		DirectoryEntry->Type = FILE_TYPE_UNKNOWN;
	else
		DirectoryEntry->Type = Ext2TypeIndicatorToFileType(Dirent.TypeIndicator);
	
	Iosb->ReadDir.NextOffset = Offset + Dirent.RecordLength;
	Iosb->ReadDir.Version = NewVersion;
	
	return IOSB_STATUS(Iosb, STATUS_SUCCESS);
}

BSTATUS Ext2ParseDir(PIO_STATUS_BLOCK Iosb, PFILE_OBJECT FileObject, const char* ParsePath, UNUSED int ParseLoopCount)
{
	PFCB InitialFcb = FileObject->Fcb;
	
	if (InitialFcb->FileType == FILE_TYPE_DIRECTORY)
	{
		if (*ParsePath == '\0' || *ParsePath == OB_PATH_SEPARATOR)
			goto ParsedThis;
		
		// This is a directory, so let's read it until we find something.
		// Note: The FCB is locked shared now.
		IO_STATUS_BLOCK Iosb2;
		IO_DIRECTORY_ENTRY DirEnt;
		BSTATUS Status;
		uint64_t Offset = 0, Version = 0;
		
		while (true)
		{
			Status = Ext2ReadDir(&Iosb2, FileObject, Offset, Version, &DirEnt);
			if (Status == STATUS_END_OF_FILE)
				break;
			
			if (FAILED(Status))
				return IOSB_STATUS(Iosb, Status);
			
			Offset  = Iosb2.ReadDir.NextOffset;
			Version = Iosb2.ReadDir.Version;
			
			size_t Match = ObMatchPathName(DirEnt.Name, ParsePath);
			if (!Match)
				continue;

			// Path name matched! Load the inode.
			PEXT2_FILE_SYSTEM FileSystem = EXT(InitialFcb)->OwnerFS;
			Status = Ext2OpenInode(FileSystem, DirEnt.InodeNumber, &Iosb->ParseDir.FoundFcb);
			if (FAILED(Status))
				return IOSB_STATUS(Iosb, Status);
			
			Iosb->ParseDir.ReparsePath = ParsePath + Match;
			return IOSB_STATUS(Iosb, STATUS_SUCCESS);
		}
		
		// Not Found
		return IOSB_STATUS(Iosb, STATUS_NAME_NOT_FOUND);
	}
	else if (InitialFcb->FileType == FILE_TYPE_SYMBOLIC_LINK)
	{
		// TODO
		ASSERT(!"Symlink Parse Not Implemented");
		return IOSB_STATUS(Iosb, STATUS_UNIMPLEMENTED);
	}
	else
	{
		// This is a file or something else, so only return success
		// if there are no remaining path components remaining.
		if (!ParsePath || *ParsePath == '\0')
		{
		ParsedThis:
			Iosb->ParseDir.FoundFcb = InitialFcb;
			Iosb->ParseDir.ReparsePath = NULL;
			Ext2ReferenceInode(InitialFcb);
			return IOSB_STATUS(Iosb, STATUS_SUCCESS);
		}
		
		return IOSB_STATUS(Iosb, STATUS_NOT_A_DIRECTORY);
	}
}
