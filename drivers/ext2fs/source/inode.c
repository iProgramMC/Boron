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

#define AcquireInodeTreeMutex(fs) ExAcquireFastMutex(&(fs)->InodeTreeMutex, false)
#define ReleaseInodeTreeMutex(fs) ExReleaseFastMutex(&(fs)->InodeTreeMutex)

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
	{
		DbgPrint("Failed to allocate intermediate buffer for ext2.  You should actually do the TODO here, bozo");
		// TODO: Use the builtin one for paging if Flags & PAGING
		return STATUS_INSUFFICIENT_MEMORY;
	}

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
			// TODO: Optimize this by using a ONE_PAGE MDL instead, in
			// case the file offset is aligned to the block size.
			uint64_t Address = BLOCK_ADDRESS(OnDiskBlock, FileSystem);
			Status = IoReadFile(Iosb, FileSystem->File, BlockBuffer, FileSystem->BlockSize, 0, Address, false, true);
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

	uint32_t ReadFlags = IO_RW_BYPASS_TYPE_CHECK;

	// Since we can only do reads at the moment, just read.
	Status = IoReadFile(Iosb, FileObject, &Dirent, sizeof(EXT2_DIRENT), ReadFlags, Offset, true, true);
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

	Status = IoReadFile(Iosb, FileObject, &DirectoryEntry->Name, NameLength, ReadFlags, Offset + sizeof(EXT2_DIRENT), true, true);
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

BSTATUS Ext2ParseDir(
	PIO_STATUS_BLOCK Iosb,
	PFILE_OBJECT FileObject,
	const char* ParsePath,
	UNUSED int ParseLoopCount,
	UNUSED PIO_PARSE_CONTEXT ParseContext
)
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

	// Call flush on the backing disk, too.
	return MmFlushFileCache(FileSystem->File->Fcb);
}

// Level 0: Only the block.
// Level 1: The block, and every block referenced by it.
// Level 2: The block, and every block referenced by it and their children.
// Level 3: More.
static BSTATUS Ext2pFreeIndirectBlockLevel(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t LevelNumber,
	uint32_t BlockIndex,
	uint64_t* BlocksFreedCount
)
{
	ASSERT(LevelNumber < 4);
	BSTATUS Status = STATUS_SUCCESS;

	if (BlockIndex == 0 || LevelNumber == 0)
	{
		// Do nothing, the caller will free us.
		return Status;
	}

	// Here's a clever "trick": reuse the temporary page sized buffer for reading too.
	// We'll split it into PAGE_SIZE/4 regions (with PAGE_SIZE/16 entries each)
	// and read chunk by chunk.

	// NOTE: This is fine, because Fast Mutexes are capable of recursive ownership.
	ExAcquireFastMutex(&FileSystem->TempPageSizeBufferMutex, false);

	uint64_t BlockAddress = BLOCK_ADDRESS(BlockIndex, FileSystem);

	size_t RegionSize = PAGE_SIZE / 4;
	if (RegionSize >= FileSystem->BlockSize)
		RegionSize = FileSystem->BlockSize;

	size_t EntriesPerRegion = RegionSize / sizeof(uint32_t);
	size_t BlockCountPerLevel = FileSystem->BlockSize / sizeof(uint32_t);
	size_t MdlOffset = LevelNumber * RegionSize;
	uint32_t* BlockIndices = FileSystem->TempPageSizeBuffer;
	BlockIndices += MdlOffset / sizeof(uint32_t);

	bool WriteBack = false;
	uint64_t WriteBackAddress = 0;

	for (size_t i = 0, j = 0; i <= BlockCountPerLevel; i++, j++)
	{
		// Read a chunk of blocks to free.
		if (j == 0 || j >= EntriesPerRegion)
		{
			if (WriteBack)
			{
				WriteBack = false;
				Status = CcWriteFileMdl(
					FileSystem->File,
					WriteBackAddress,
					&FileSystem->TempPageSizeBufferMdl.Base,
					MdlOffset,
					RegionSize
				);

				if (FAILED(Status))
				{
					DbgPrint("EXT2: Failed to write to block list: %s", RtlGetStatusString(Status));
					break;
				}

			}

			// We wanted to reach BlockCountPerLevel because we wanted to ensure writeback was being
			// performed for the last block.
			if (i == BlockCountPerLevel) {
				break;
			}

			Status = CcReadFileMdl(
				FileSystem->File,
				BlockAddress,
				&FileSystem->TempPageSizeBufferMdl.Base,
				MdlOffset,
				RegionSize
			);

			if (FAILED(Status))
			{
				DbgPrint("EXT2: Failed to read from block list: %s", RtlGetStatusString(Status));
				break;
			}

			j = 0;
			WriteBackAddress = BlockAddress;
			BlockAddress += RegionSize;
		}

		ASSERT(i < BlockCountPerLevel);

		if (BlockIndices[j] == 0) {
			// Possibly sparsely allocated, so fine.
			continue;
		}

		Status = Ext2pFreeIndirectBlockLevel(
			FileSystem,
			LevelNumber - 1,
			BlockIndices[j],
			BlocksFreedCount
		);

		if (FAILED(Status))
		{
			DbgPrint("EXT2: Failed to free indirection block %u: %s", BlockIndices[j], RtlGetStatusString(Status));
			break;
		}

		Status = Ext2FreeBlock(FileSystem, BlockIndices[j]);
		if (FAILED(Status))
		{
			DbgPrint("EXT2: Failed to free block %u: %s", BlockIndices[j], RtlGetStatusString(Status));
			break;
		}

		BlockIndices[j] = 0;
		(*BlocksFreedCount)++;
		WriteBack = true;
	}

	ExReleaseFastMutex(&FileSystem->TempPageSizeBufferMutex);
	return Status;
}

static BSTATUS Ext2pFreePartialIndirectBlockLevel(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t LevelNumber,
	uint32_t BlockIndex,
	uint64_t TruncationOffsetWithinThisLevel,
	uint64_t* BlocksFreedCount
)
{
	ASSERT(LevelNumber < 4);
	BSTATUS Status = STATUS_SUCCESS;

	if (BlockIndex == 0 || LevelNumber == 0)
	{
		return Status;
	}

	// Here's a clever "trick": reuse the temporary page sized buffer for reading too.
	// We'll split it into PAGE_SIZE/4 regions (with PAGE_SIZE/16 entries each)
	// and read chunk by chunk.

	// NOTE: This is fine, because Fast Mutexes are capable of recursive ownership.
	ExAcquireFastMutex(&FileSystem->TempPageSizeBufferMutex, false);

	uint64_t BlockAddress = BLOCK_ADDRESS(BlockIndex, FileSystem);

	size_t RegionSize = PAGE_SIZE / 4;
	if (RegionSize >= (size_t) FileSystem->BlockSize)
		RegionSize = FileSystem->BlockSize;

	size_t EntriesPerRegion = RegionSize / sizeof(uint32_t);
	size_t BlockCountPerLevel = FileSystem->BlockSize / sizeof(uint32_t);
	size_t MdlOffset = LevelNumber * RegionSize;
	uint32_t* BlockIndices = FileSystem->TempPageSizeBuffer;
	BlockIndices += MdlOffset / sizeof(uint32_t);

	uint64_t BlocksCoveredByLevel = 1, BlocksCoveredByNextLevel = 1;
	for (uint32_t i = 0; i < LevelNumber; i++) {
		BlocksCoveredByNextLevel = BlocksCoveredByLevel;
		BlocksCoveredByLevel *= BlockCountPerLevel;
	}

	ASSERT(BlocksCoveredByLevel != BlocksCoveredByNextLevel);

	bool WriteBack = false;
	uint64_t WriteBackAddress = 0;

	for (size_t i = 0, j = 0; i <= BlockCountPerLevel; i++, j++)
	{
		// Read a chunk of blocks to free.
		if (j == 0 || j >= EntriesPerRegion)
		{
			if (WriteBack)
			{
				WriteBack = false;
				Status = CcWriteFileMdl(
					FileSystem->File,
					WriteBackAddress,
					&FileSystem->TempPageSizeBufferMdl.Base,
					MdlOffset,
					RegionSize
				);

				if (FAILED(Status))
				{
					DbgPrint("EXT2: Failed to write to block list: %s", RtlGetStatusString(Status));
					break;
				}
			}

			// We wanted to reach BlockCountPerLevel because we wanted to ensure writeback was being
			// performed for the last block.
			if (i == BlockCountPerLevel) {
				break;
			}

			Status = CcReadFileMdl(
				FileSystem->File,
				BlockAddress,
				&FileSystem->TempPageSizeBufferMdl.Base,
				MdlOffset,
				RegionSize
			);

			if (FAILED(Status))
			{
				DbgPrint("EXT2: Failed to read from block list: %s", RtlGetStatusString(Status));
				break;
			}

			j = 0;
			WriteBackAddress = BlockAddress;
			BlockAddress += RegionSize;
		}

		ASSERT(i < BlockCountPerLevel);

		if (BlockIndices[j] == 0) {
			// Possibly sparsely allocated, so fine.
			continue;
		}

		uint64_t Start = i * BlocksCoveredByNextLevel;
		uint64_t End = (i + 1) * BlocksCoveredByNextLevel;

		if (End <= TruncationOffsetWithinThisLevel)
		{
			// Do nothing.
			Status = STATUS_SUCCESS;
		}
		else if (Start >= TruncationOffsetWithinThisLevel)
		{
			// Free everything.
			Status = Ext2pFreeIndirectBlockLevel(
				FileSystem,
				LevelNumber - 1,
				BlockIndices[j],
				BlocksFreedCount
			);

			if (SUCCEEDED(Status))
			{
				Status = Ext2FreeBlock(FileSystem, BlockIndices[j]);

				if (SUCCEEDED(Status))
				{
					(*BlocksFreedCount)++;
					BlockIndices[j] = 0;
					WriteBack = true;
				}
			}
		}
		else
		{
			// Partially free.
			//
			// The whole block won't be freed, but it will be modified.
			Status = Ext2pFreePartialIndirectBlockLevel(
				FileSystem,
				LevelNumber - 1,
				BlockIndices[j],
				TruncationOffsetWithinThisLevel - Start,
				BlocksFreedCount
			);
		}

		if (FAILED(Status))
		{
			DbgPrint("EXT2: Failed to free block %u: %s", BlockIndices[j], RtlGetStatusString(Status));
			break;
		}
	}

	ExReleaseFastMutex(&FileSystem->TempPageSizeBufferMutex);
	return Status;
}

static void Ext2pSetFileSizeFcb(PFCB Fcb, uint64_t NewLength)
{
	PREP_EXT;
	PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;

	Fcb->FileLength = NewLength;

	Ext->InodeDataIsDirty = true;
	Ext->Inode.Size = (uint32_t)(NewLength & 0xFFFFFFFF);

	if (FileSystem->SuperBlock.Version > 0)
		Ext->Inode.SizeUpper = (uint32_t)(NewLength >> 32);
}

BSTATUS Ext2TouchFile(PFCB Fcb, bool IsWrite)
{
	// TODO
	DbgPrint("TODO: Ext2TouchFile(%p, %s)", Fcb, IsWrite ? "WRITE" : "READ");
	return STATUS_SUCCESS;
}

BSTATUS Ext2ResizeFile(PFCB Fcb, uint64_t NewLength)
{
	BSTATUS Status = STATUS_SUCCESS;
	PREP_EXT;
	PEXT2_FILE_SYSTEM FileSystem = Ext->OwnerFS;

	if (FileSystem->ForceReadOnly)
	{
		return STATUS_WRITE_PROTECTED;
	}

	if (NewLength >= 0x100000000 && FileSystem->SuperBlock.Version == 0)
	{
		DbgPrint("Ext2Resize: Cannot create a file > 4 GiB on version 0 ext2 partitions.");
		return STATUS_INSUFFICIENT_SPACE;
	}

	AcquireBlockRwlockExclusive(Ext);

	uint64_t CurrentBlockCount = (Fcb->FileLength + FileSystem->BlockSize - 1) / FileSystem->BlockSize;
	uint64_t NewBlockCount = (NewLength + FileSystem->BlockSize - 1) / FileSystem->BlockSize;

	if (CurrentBlockCount == NewBlockCount)
	{
		Fcb->FileLength = NewLength;

		ExAcquireFastMutex(&Ext->InodeDataMutex, false);

		Ext2pSetFileSizeFcb(Fcb, NewLength);

		if (FileSystem->BlockSize > PAGE_SIZE) {
			// If the block size is LARGER than the page size, truncation may still be
			// necessary.
			MmTruncateFileCache(Fcb, NewLength);
		}

		ExReleaseFastMutex(&Ext->InodeDataMutex);
		ReleaseBlockRwlock(Ext);
		return STATUS_SUCCESS;
	}

	// Are we shrinking the file?
	if (CurrentBlockCount > NewBlockCount)
	{
		// Determine which indirection regions are worth keeping.
		ExAcquireFastMutex(&Ext->InodeDataMutex, false);

		// Do the OS-level rigmarole of decreasing the size already.
		Ext2pSetFileSizeFcb(Fcb, NewLength);
		MmTruncateFileCache(Fcb, NewLength);

		size_t EntriesPerBlock = FileSystem->BlockSize / sizeof(uint32_t);
		uint64_t LevelLimits[4];
		LevelLimits[0] = 12;
		LevelLimits[1] = LevelLimits[0] + (uint64_t) EntriesPerBlock;
		LevelLimits[2] = LevelLimits[1] + (uint64_t) EntriesPerBlock * EntriesPerBlock;
		LevelLimits[3] = LevelLimits[2] + (uint64_t) EntriesPerBlock * EntriesPerBlock * EntriesPerBlock;

		uint64_t BlocksFreedCount = 0;

		// Level 0.
		for (uint64_t i = 0; i < 12; i++)
		{
			if (NewBlockCount <= i)
			{
				if (Ext->Inode.DirectBlockPointer[i] == 0)
					continue;

				Status = Ext2FreeBlock(FileSystem, Ext->Inode.DirectBlockPointer[i]);
				if (FAILED(Status))
					break;

				Ext->Inode.DirectBlockPointer[i] = 0;
				Ext->InodeDataIsDirty = true;
				BlocksFreedCount++;
			}
		}

		if (FAILED(Status))
			goto Fail;

		// Levels 1-3.
		for (int Level = 1; Level <= 3; Level++)
		{
			uint32_t* Slot = &Ext->Inode.BlockPointers[12 + Level - 1];
			if (*Slot == 0) {
				// Unallocated.
				continue;
			}

			uint64_t Start = LevelLimits[Level - 1];
			uint64_t End = LevelLimits[Level];

			if (NewBlockCount >= End) {
				// Keep this level.
				continue;
			}

			if (NewBlockCount <= Start)
			{
				Status = Ext2pFreeIndirectBlockLevel(
					FileSystem,
					Level,
					*Slot,
					&BlocksFreedCount
				);
				if (FAILED(Status))
					break;

				Status = Ext2FreeBlock(FileSystem, *Slot);
				if (FAILED(Status))
					break;

				*Slot = 0;
				Ext->InodeDataIsDirty = true;
				BlocksFreedCount++;
				continue;
			}

			// Partial free.
			Status = Ext2pFreePartialIndirectBlockLevel(
				FileSystem,
				Level,
				*Slot,
				NewBlockCount - Start,
				&BlocksFreedCount
			);

			if (FAILED(Status))
				break;
		}

	Fail:
		// This should NEVER run out of memory. Out of memory should trigger the flushing of file
		// pages to their backing store (which doesn't call this function in the first place), and
		// BLOCK this thread until there is sufficient memory.  The only failures we expect to see
		// here are hardware I/O errors, which are not considered recoverable (any attempt to access
		// the volume will result in another failure, either way).
		ASSERT(Status != STATUS_INSUFFICIENT_MEMORY);

		if (SUCCEEDED(Status))
		{
			uint32_t SectorsPerBlock = FileSystem->BlockSize / 512;
			uint32_t SectorsFreed = (uint32_t)(SectorsPerBlock * BlocksFreedCount);

			if (Ext->Inode.BlockCount < SectorsFreed) {
				DbgPrint("Ext2: Accounting error: Sector count is %u, but freed %u sectors", Ext->Inode.BlockCount, SectorsFreed);
				Ext->Inode.BlockCount = 0;
			}
			else {
				Ext->Inode.BlockCount -= SectorsFreed;
			}
		}
		else
		{
			DbgPrint("Ext2: Could not resize file: %s", RtlGetStatusString(Status));
			DbgPrint("Ext2: You might need to run 'fsck' on this partition.");
		}

		ExReleaseFastMutex(&Ext->InodeDataMutex);
		ReleaseBlockRwlock(Ext);
		return Status;
	}

	DbgPrint("Ext2: TODO: Ext2ResizeFile with allocating blocks.");
	ReleaseBlockRwlock(Ext);

	return STATUS_UNIMPLEMENTED;
}
