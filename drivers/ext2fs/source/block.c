/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	block.c

Abstract:
	This module manages Ext2 block and inode allocation.

Author:
	iProgramInCpp - 3 November 2026
***/
#include "ext2.h"

BSTATUS Ext2ReadBlockGroupDescriptor(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockGroup,
	PEXT2_BLOCK_GROUP_DESCRIPTOR Descriptor
)
{
	// If the block's size is 1024, then the block group descriptor table
	// is at block 2, otherwise it's bigger, so at block 1.
	uint64_t Address = FileSystem->BlockSize;
	if (Address == 1024)
		Address = 2048;

	Address += BlockGroup * sizeof(EXT2_BLOCK_GROUP_DESCRIPTOR);

	return CcReadFileCopy(
		FileSystem->File,
		Address,
		Descriptor,
		sizeof *Descriptor
	);
}

BSTATUS Ext2WriteBlockGroupDescriptor(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockGroup,
	PEXT2_BLOCK_GROUP_DESCRIPTOR Descriptor
)
{
	// If the block's size is 1024, then the block group descriptor table
	// is at block 2, otherwise it's bigger, so at block 1.
	uint64_t Address = FileSystem->BlockSize;
	if (Address == 1024)
		Address = 2048;

	Address += BlockGroup * sizeof(EXT2_BLOCK_GROUP_DESCRIPTOR);

	return CcWriteFileCopy(
		FileSystem->File,
		Address,
		Descriptor,
		sizeof *Descriptor
	);
}

static uint32_t Ext2pGetBlockGroupForBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockIndex,
	uint32_t* OutBlockIndexInGroup)
{
	ASSERT(BlockIndex != 0 && "Block 0 is not a valid block number.");

	BlockIndex -= FileSystem->SuperBlock.FirstDataBlock;

	uint32_t BlocksPerGroup = FileSystem->SuperBlock.BlocksPerGroup;
	*OutBlockIndexInGroup = BlockIndex % BlocksPerGroup;
	return BlockIndex / BlocksPerGroup;
}

static uint32_t Ext2pGetFirstBlockInGroup(PEXT2_FILE_SYSTEM FileSystem, uint32_t BlockGroupIndex)
{
	return BlockGroupIndex * FileSystem->SuperBlock.BlocksPerGroup + FileSystem->SuperBlock.FirstDataBlock;
}

// NOTE: This is probably not the most efficient allocation algorithm, but it should ensure locality
// within a block group to the greatest extent possible, which is important.
static BSTATUS Ext2pAllocateBlocksInBlockGroup(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockGroupIndex,
	uint32_t BlockIndex,
	size_t RequestedBlockCount,
	uint32_t* OutBlockStart,
	size_t* OutBlockCount,
	uint32_t BlockGroupCount
)
{
	BSTATUS Status;

	EXT2_BLOCK_GROUP_DESCRIPTOR Descriptor;

	// Read the block group descriptor.
	Status = Ext2ReadBlockGroupDescriptor(
		FileSystem,
		BlockGroupIndex,
		&Descriptor
	);

	if (FAILED(Status))
	{
		DbgPrint("%s: Failed to read block group descriptor: %s", __func__, RtlGetStatusString(Status));
		return Status;
	}

	// If there are no free blocks, then don't bother trying to allocate anything.
	if (Descriptor.FreeBlockCount == 0)
	{
		return STATUS_INSUFFICIENT_SPACE;
	}

	// Read the bitmap.
	ExAcquireFastMutex(&FileSystem->TempBlockSizeBufferMutex, false);

	uint64_t BlockBitmapAddress = (uint64_t) FileSystem->BlockSize * Descriptor.BlockBitmapBlockId;
	Status = CcReadFileMdl(
		FileSystem->File,
		BlockBitmapAddress,
		FileSystem->TempBlockSizeBufferMdl,
		0, // MdlOffset
		FileSystem->BlockSize
	);

	if (FAILED(Status))
	{
		DbgPrint("%s: Failed to read block bitmap: %s", __func__, RtlGetStatusString(Status));
		ExReleaseFastMutex(&FileSystem->TempBlockSizeBufferMutex);
		return Status;
	}

	// Allocation parameters
	uint32_t FirstBlock = 0;
	size_t BlockCount = 0;
	bool EndAllocation = false;

	int InitialOffset = (BlockIndex / 8) & ~(FileSystem->BlockSize - 1);
	int ReadOffset = InitialOffset;

	uint32_t BlocksInThisBitmap = FileSystem->BlockSize * 8;
	if (BlockGroupIndex == BlockGroupCount - 1)
	{
		// The last block is a special case.
		uint32_t FirstInGroup = Ext2pGetFirstBlockInGroup(FileSystem, BlockGroupIndex);
		BlocksInThisBitmap = FileSystem->SuperBlock.BlockCount - FirstInGroup;

		if (BlocksInThisBitmap >= FileSystem->SuperBlock.BlocksPerGroup)
			BlocksInThisBitmap = FileSystem->SuperBlock.BlocksPerGroup;
	}

	if (BlockIndex >= BlocksInThisBitmap)
		BlockIndex = 0;

	ASSERT(BlocksInThisBitmap != 0);

	uint64_t* Bitmap = FileSystem->TempBlockSizeBuffer;
	bool WriteBack = false;

	// TODO: If BlockIndex is not divisible by 64, process entries after BlockIndex
	// before entries before BlockIndex within the same bitmap group.
	BlockIndex &= ~63;

	// Scan through all the entries in the bitmap.
	uint32_t InitialBlockIndex = BlockIndex;
	do
	{
		uint32_t BlockIndexEnd = BlockIndex + 64;
		if (BlockIndexEnd >= BlocksInThisBitmap)
			BlockIndexEnd = BlocksInThisBitmap;

		int BitsToCount = BlockIndexEnd - BlockIndex;
		int BitmapIndex = BlockIndex / 64;
		uint64_t Bits = Bitmap[BitmapIndex];

		// TODO: For big-endian, perform byteswap here...

		for (int i = 0; i < BitsToCount; i++)
		{
			if ((Bits >> i) & 1)
			{
				// If we were doing an allocation, stop it now.
				if (BlockCount > 0) {
					EndAllocation = true;
					break;
				}

				// Otherwise, skip this block index.
				continue;
			}

			// Found a free region!  This loop will continue until we run into an
			// allocated block, or we exhaust the limit.
			BlockCount++;
			if (BlockCount == 1) {
				FirstBlock = BlockIndex + i;
			}

			// Mark it as allocated, too.
			Bits |= 1ULL << i;
			WriteBack = true;

			// If we ran into the limit of allocated blocks, return now.
			if (BlockCount >= RequestedBlockCount) {
				ASSERT(BlockCount == RequestedBlockCount);
				BlockCount = RequestedBlockCount;
				EndAllocation = true;
				break;
			}
		}

		if (WriteBack) {
			// TODO: For big endian... and here.
			Bitmap[BitmapIndex] = Bits;
		}

		BlockIndex += 64;
		if (BlockIndex >= BlocksInThisBitmap)
		{
			BlockIndex = 0;

			if (BlockCount > 0) {
				EndAllocation = true;
				break;
			}
		}
	}
	while (BlockIndex != InitialBlockIndex && !EndAllocation);

	if (WriteBack)
	{
		// Write the modified bitmap back to the cache.
		Status = CcWriteFileMdl(
			FileSystem->File,
			BlockBitmapAddress + ReadOffset,
			FileSystem->TempBlockSizeBufferMdl,
			0, // MdlOffset
			FileSystem->BlockSize
		);

		if (FAILED(Status))
		{
			DbgPrint("%s: Failed to write block bitmap: %s", __func__, RtlGetStatusString(Status));
			ExReleaseFastMutex(&FileSystem->TempBlockSizeBufferMutex);
			return Status;
		}
	}

	if (BlockCount == 0)
	{
		// Insufficient space inside this block group.  Try the next one.
		ExReleaseFastMutex(&FileSystem->TempBlockSizeBufferMutex);
		return STATUS_INSUFFICIENT_SPACE;
	}

	Status = Ext2WriteBlockGroupDescriptor(
		FileSystem,
		BlockGroupIndex,
		&Descriptor
	);

	if (FAILED(Status))
	{
		// TODO: What are we even supposed to do in this situation?
		DbgPrint(
			"Ext2: ERROR: Cannot write back the modified block group descriptor. %s",
			RtlGetStatusString(Status)
		);
	}

	ExReleaseFastMutex(&FileSystem->TempBlockSizeBufferMutex);

	// Update this block group descriptor's allocated blocks count, as well as the super block's.
	ASSERT(BlockCount < 65535 && "The amount of blocks allocated shouldn't have exceeded the max "
		"from the block group descriptor");
	Ext2DecrementFreeBlocksSuperBlock(FileSystem, BlockCount);

	if (Descriptor.FreeBlockCount < BlockCount)
	{
		DbgPrint(
			"Ext2: Accounting error: There are supposedly %u blocks free in this block group, "
			"but we're trying to allocate %zu blocks.  Run 'fsck' on your ext2 partition.",
			Descriptor.FreeBlockCount,
			BlockCount
		);
		Descriptor.FreeBlockCount = 0;
	}
	else
	{
		Descriptor.FreeBlockCount -= BlockCount;
	}

	*OutBlockStart = FirstBlock;
	*OutBlockCount = BlockCount;
	return STATUS_SUCCESS;
}

//
// Allocates one or more blocks within an ext2 partition.  If no blocks were allocated, returns
// zero.  If the allocation succeeds, the first block is returned and the number of allocated
// blocks is returned in OutBlockCount.  Every block inside the [BlockStart, BlockStart+Count-1]
// range will have been allocated by this function.  The OutBlockCount value will never return
// more than BlockCount blocks.
//
// This routine is meant to be called in a loop.  For example, if requesting to allocate 60 blocks,
// this function could return the first 20, then the next 12, then the next 24, then the last 4.
//
// This routine will NEVER return blocks spanning different block groups. (It's actually kind of
// impossible, even if we wanted to, because the next block group always starts with allocated
// blocks)
//
BSTATUS Ext2AllocateBlocks(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockHint,
	size_t BlockCount,
	uint32_t* OutBlockStart,
	size_t* OutBlockCount
)
{
	BSTATUS Status = STATUS_SUCCESS;
	if (BlockCount == 0) {
		*OutBlockStart = 0;
		*OutBlockCount = 0;
		return STATUS_SUCCESS;
	}

	ExAcquireFastMutex(&FileSystem->BlockAllocationMutex, false);

	*OutBlockCount = 0;

	// If a block hint was provided, start by allocating within the block group.
	uint32_t BlocksPerGroup = FileSystem->SuperBlock.BlocksPerGroup;
	uint32_t BlockGroupCount =
		(FileSystem->SuperBlock.BlockCount
			- FileSystem->SuperBlock.FirstDataBlock
			+ BlocksPerGroup - 1)
		/ BlocksPerGroup;
	uint32_t BlockStartInGroup = 0;

	if (BlockHint != 0)
	{
		uint32_t BlockGroup, BlockIndexInGroup;
		BlockGroup = Ext2pGetBlockGroupForBlock(
			FileSystem,
			BlockHint,
			&BlockIndexInGroup
		);

		uint32_t FirstBlockInGroup = Ext2pGetFirstBlockInGroup(FileSystem, BlockGroup);
		Status = Ext2pAllocateBlocksInBlockGroup(
			FileSystem,
			BlockGroup,
			BlockIndexInGroup,
			BlockCount,
			&BlockStartInGroup,
			OutBlockCount,
			BlockGroupCount
		);

		if (Status != STATUS_INSUFFICIENT_SPACE)
		{
			ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
			if (SUCCEEDED(Status)) {
				*OutBlockStart = FirstBlockInGroup + BlockStartInGroup;
			}
			return Status;
		}

		// Allocation didn't succeed, try allocating more blocks from the next block group
		uint32_t InitialBlockGroup = BlockGroup;
		BlockGroup++;
		if (BlockGroup >= BlockGroupCount) {
			BlockGroup = 0;
		}

		while (InitialBlockGroup != BlockGroup)
		{
			uint32_t FirstBlockInGroup = Ext2pGetFirstBlockInGroup(FileSystem, BlockGroup);

			Status = Ext2pAllocateBlocksInBlockGroup(
				FileSystem,
				BlockGroup,
				0, // BlockIndex
				BlockCount,
				&BlockStartInGroup,
				OutBlockCount,
				BlockGroupCount
			);

			if (Status != STATUS_INSUFFICIENT_SPACE)
			{
				ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
				if (SUCCEEDED(Status)) {
					*OutBlockStart = FirstBlockInGroup + BlockStartInGroup;
				}
				return Status;
			}

			BlockGroup++;
			if (BlockGroup >= BlockGroupCount) {
				BlockGroup = 0;
			}
		}

		ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
		DbgPrint("Ext2AllocateBlocks  ERROR: Could not allocate blocks! Out of blocks to allocate! (hint given)");
		return STATUS_INSUFFICIENT_SPACE;
	}

	for (uint32_t BlockGroup = 0; BlockGroup < BlockGroupCount; BlockGroup++)
	{
		uint32_t FirstBlockInGroup = Ext2pGetFirstBlockInGroup(FileSystem, BlockGroup);

		Status = Ext2pAllocateBlocksInBlockGroup(
			FileSystem,
			BlockGroup,
			0, // BlockIndex
			BlockCount,
			&BlockStartInGroup,
			OutBlockCount,
			BlockGroupCount
		);

		if (Status != STATUS_INSUFFICIENT_SPACE)
		{
			ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
			if (SUCCEEDED(Status)) {
				*OutBlockStart = FirstBlockInGroup + BlockStartInGroup;
			}
			return Status;
		}
	}

	ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
	DbgPrint("Ext2AllocateBlocks  ERROR: Could not allocate blocks! Out of blocks to allocate! (hint not given)");
	return STATUS_INSUFFICIENT_SPACE;
}

// TODO: Allow for freeing multiple blocks in a batch or range.
BSTATUS Ext2FreeBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t BlockIndex
)
{
	BSTATUS Status;
	uint32_t BlockGroup, BlockIndexInGroup;
	BlockGroup = Ext2pGetBlockGroupForBlock(
		FileSystem,
		BlockIndex,
		&BlockIndexInGroup
	);

	ExAcquireFastMutex(&FileSystem->BlockAllocationMutex, false);

	// Read the block group descriptor now.
	EXT2_BLOCK_GROUP_DESCRIPTOR Descriptor;
	Status = Ext2ReadBlockGroupDescriptor(
		FileSystem,
		BlockGroup,
		&Descriptor
	);

	if (FAILED(Status))
	{
		ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
		DbgPrint("Ext2: Cannot read block group descriptor while freeing: %s", RtlGetStatusString(Status));
		return Status;
	}

	// This check is incomplete, and that's OK.
	ASSERT(BlockIndex != Descriptor.InodeTableBlockId && "Why are we trying to free the inode table?");

	// These checks are complete, though, since the block bitmap and table are both
	// 1 block in size.
	ASSERT(BlockIndex != Descriptor.BlockBitmapBlockId && "Why are we trying to free the block bitmap?");
	ASSERT(BlockIndex != Descriptor.InodeBitmapBlockId && "Why are we trying to free the inode bitmap?");

	uint64_t Address = (uint64_t) Descriptor.BlockBitmapBlockId * FileSystem->BlockSize;
	Address += BlockIndexInGroup / 8;

	int BitIndex = BlockIndexInGroup % 8;
	uint8_t AllocationMask = (1 << BitIndex);

	uint8_t AllocationBitmap = 0;

	// Since CcReadFile/CcWriteFile support arbitrary sized writes (even if the underlying block
	// devices don't), we can just do this, and it'll translate to a block-size write later.
	Status = CcReadFileCopy(
		FileSystem->File,
		Address,
		&AllocationBitmap,
		sizeof AllocationBitmap
	);

	if (FAILED(Status))
	{
		ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
		DbgPrint("Ext2: Cannot read allocation bitmap while freeing: %s", RtlGetStatusString(Status));
		return Status;
	}

	if (!(AllocationBitmap & AllocationMask))
	{
		ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
		DbgPrint("Ext2: Cannot free already freed block %u", BlockIndex);
		return STATUS_HARDWARE_IO_ERROR;
	}

	AllocationBitmap &= ~AllocationMask;
	Descriptor.FreeBlockCount++;
	ASSERT(Descriptor.FreeBlockCount != 0 && "Block count overflow!");

	// Write it back now.
	Status = CcWriteFileCopy(
		FileSystem->File,
		Address,
		&AllocationBitmap,
		sizeof AllocationBitmap
	);

	if (FAILED(Status))
	{
		ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);
		DbgPrint("Ext2: Cannot write allocation bitmap while freeing: %s", RtlGetStatusString(Status));
		return Status;
	}

	// Write the descriptor too. However, a failure in this routine isn't fatal.
	Status = Ext2WriteBlockGroupDescriptor(
		FileSystem,
		BlockGroup,
		&Descriptor
	);

	if (FAILED(Status))
		DbgPrint("Ext2: Cannot rewrite block group descriptor while freeing: %s", RtlGetStatusString(Status));

	ExReleaseFastMutex(&FileSystem->BlockAllocationMutex);

	Ext2IncrementFreeBlocksSuperBlock(FileSystem, 1);

	return STATUS_SUCCESS;
}
