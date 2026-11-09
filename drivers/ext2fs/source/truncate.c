/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	truncate.c

Abstract:
	This module manages Ext2 file truncation/growing logic.

Author:
	iProgramInCpp - 9 November 2026
***/
#include "ext2.h"
#include "inode.h"

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
