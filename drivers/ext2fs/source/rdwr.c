/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	rdwr.c

Abstract:
	This module manages Ext2 file reading and writing logic.

Author:
	iProgramInCpp - 11 November 2026
***/
#include "ext2.h"
#include "inode.h"

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
		Status = Ext2pFindOnDiskBlock(Fcb, BlockIndex, &OnDiskBlock, false);
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
