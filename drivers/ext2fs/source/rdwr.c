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

	if (Fcb->FileType == FILE_TYPE_SYMBOLIC_LINK && Fcb->FileLength <= 15 * sizeof(uint32_t))
	{
		// TODO: Support symbolic links.
		//
		// Links shorter than or equal to 60 characters place their contents directly inside the
		// list of block pointers, rather than using a block to store their contents.
		DbgPrint("Ext2: Reading from a symbolic link shorter than 60 chars is not supported");
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	}

	if (Size + Offset >= FileSize)
		Size = FileSize - Offset;

	uint8_t* BlockBuffer;
	PMDL BlockBufferMdl;
	bool UsingTempBlockSizeBuffer = false;

	// Try to allocate a buffer.
	//
	// TODO: Don't try if memory pressure is high and we're paging
	BlockBuffer = MmAllocatePool(POOL_NONPAGED, FileSystem->BlockSize);
	if (BlockBuffer)
	{
		Status = MmCreateMdl(
			&BlockBufferMdl,
			(uintptr_t) BlockBuffer,
			FileSystem->BlockSize,
			KeGetPreviousMode(),
			true  // IsWrite
		);

		if (FAILED(Status)) {
			MmFreePool(BlockBuffer);
			BlockBuffer = NULL;
		}
	}

	if (!BlockBuffer)
	{
		// Use the existing one.
		UsingTempBlockSizeBuffer = true;

		ExAcquireFastMutex(&FileSystem->TempBlockSizeBufferMutex, false);
		BlockBuffer = FileSystem->TempBlockSizeBuffer;
		BlockBufferMdl = FileSystem->TempBlockSizeBufferMdl;
	}

	while (Size)
	{
		// Which block index is this offset within?
		uint32_t BlockIndex = Offset >> FileSystem->BlockSizeLog2;
		uint32_t OnDiskBlock = 0;

		AcquireBlockRwlockShared(Ext);

		Status = Ext2FindOnDiskBlock(Fcb, BlockIndex, &OnDiskBlock, false, NULL);

		if (FAILED(Status)) {
			ReleaseBlockRwlock(Ext);
			break;
		}

		uint32_t BlockOffset = Offset & (FileSystem->BlockSize - 1);

		size_t BytesTillNext = FileSystem->BlockSize - BlockOffset;
		size_t CopyAmount = Size;
		if (CopyAmount > BytesTillNext)
			CopyAmount = BytesTillNext;

		// Okay, now do the read itself.
		if (OnDiskBlock)
		{
			uint64_t Address = BLOCK_ADDRESS(OnDiskBlock, FileSystem);
			Status = IoReadFileMdl(
				Iosb,             // Iosb
				FileSystem->File, // FileObject
				BlockBufferMdl,   // Mdl
				0,                // Flags
				Address,          // FileOffset
				false             // Cached
			);

			if (FAILED(Status)) {
				ReleaseBlockRwlock(Ext);
				break;
			}

			MmCopyIntoMdl(MdlBuffer, MdlOffset, BlockBuffer + BlockOffset, CopyAmount);
		}
		else
		{
			MmSetIntoMdl(MdlBuffer, MdlOffset, 0, CopyAmount);
		}

		// I think we need to release it here to prevent a class of race condition where the block
		// we're reading could be reused before we get to read data from it.  But I could be wrong.
		ReleaseBlockRwlock(Ext);

		MdlOffset += CopyAmount;
		Offset += CopyAmount;
		BytesRead += CopyAmount;
		Size -= CopyAmount;
	}

	if (UsingTempBlockSizeBuffer)
	{
		ExReleaseFastMutex(&FileSystem->TempBlockSizeBufferMutex);
	}
	else
	{
		MmFreeMdl(BlockBufferMdl);
		MmFreePool(BlockBuffer);
	}

	Iosb->BytesRead = BytesRead;
	Iosb->Status = Status;

	MmFreePool(BlockBuffer);
	return Status;
}

BSTATUS Ext2Write(PIO_STATUS_BLOCK Iosb, PFCB Fcb, uint64_t Offset, PMDL MdlBuffer, UNUSED uint32_t Flags)
{
	BSTATUS Status;
	size_t Size, MdlOffset;
	size_t BytesRead = 0;
	PEXT2_FILE_SYSTEM FileSystem;

	PREP_EXT;
	FileSystem = Ext->OwnerFS;

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

	if (Fcb->FileType == FILE_TYPE_SYMBOLIC_LINK && Fcb->FileLength <= 15 * sizeof(uint32_t))
	{
		// TODO: Support symbolic links.
		//
		// Links shorter than or equal to 60 characters place their contents directly inside the
		// list of block pointers, rather than using a block to store their contents.
		DbgPrint("Ext2: Writing to a symbolic link shorter than 60 chars is not supported");
		return IOSB_STATUS(Iosb, STATUS_UNSUPPORTED_FUNCTION);
	}

	if (Size + Offset >= FileSize)
		Size = FileSize - Offset;

	uint8_t* BlockBuffer;
	PMDL BlockBufferMdl;
	bool UsingTempBlockSizeBuffer = false;

	// Try to allocate a buffer.
	//
	// TODO: Don't try if memory pressure is high and we're paging
	BlockBuffer = MmAllocatePool(POOL_NONPAGED, FileSystem->BlockSize);
	if (BlockBuffer)
	{
		Status = MmCreateMdl(
			&BlockBufferMdl,
			(uintptr_t) BlockBuffer,
			FileSystem->BlockSize,
			KeGetPreviousMode(),
			true  // IsWrite
		);

		if (FAILED(Status)) {
			MmFreePool(BlockBuffer);
			BlockBuffer = NULL;
		}
	}

	if (!BlockBuffer)
	{
		// Use the existing one.
		UsingTempBlockSizeBuffer = true;

		ExAcquireFastMutex(&FileSystem->TempBlockSizeBufferMutex, false);
		BlockBuffer = FileSystem->TempBlockSizeBuffer;
		BlockBufferMdl = FileSystem->TempBlockSizeBufferMdl;
	}

	while (Size)
	{
		// Which block index is this offset within?
		uint32_t BlockIndex = Offset >> FileSystem->BlockSizeLog2;
		uint32_t OnDiskBlock = 0;

		AcquireBlockRwlockShared(Ext);

		Status = Ext2FindOnDiskBlock(Fcb, BlockIndex, &OnDiskBlock, false, NULL);

		if (FAILED(Status)) {
			ReleaseBlockRwlock(Ext);
			break;
		}

		bool ClearThisBlockToZero = false;

		// If the block is zero, we need to allocate it.
		if (OnDiskBlock == 0)
		{
			// Note: Since we're releasing the lock we need to go through the entire
			// routine again.  Somebody else could've allocated blocks in our place,
			// but the Ext2pFindOnDiskBlock routine will have caught this.
			ReleaseBlockRwlock(Ext);
			AcquireBlockRwlockExclusive(Ext);

			// NOTE: this will also fill in the correct entry number, too.
			Status = Ext2FindOnDiskBlock(Fcb, BlockIndex, &OnDiskBlock, true, &ClearThisBlockToZero);
			if (FAILED(Status)) {
				ReleaseBlockRwlock(Ext);
				break;
			}

			DemoteBlockRwlockToShared(Ext);
		}

		uint32_t BlockOffset = Offset & (FileSystem->BlockSize - 1);

		size_t BytesTillNext = FileSystem->BlockSize - BlockOffset;
		size_t CopyAmount = Size;
		if (CopyAmount > BytesTillNext)
			CopyAmount = BytesTillNext;

		uint64_t Address = BLOCK_ADDRESS(OnDiskBlock, FileSystem);

		// First, check if we depend on the contents of this block at all.
		if (BlockOffset == 0 && CopyAmount == FileSystem->BlockSize)
		{
			// We do not.
		}
		else if (ClearThisBlockToZero)
		{
			memset(BlockBuffer, 0, FileSystem->BlockSize);
		}
		else
		{
			// Read the block before modifying it.
			Status = IoReadFileMdl(
				Iosb,
				FileSystem->File,
				BlockBufferMdl,
				0,                // Flags
				Address,          // FileOffset
				false             // Cached
			);

			if (FAILED(Status)) {
				ReleaseBlockRwlock(Ext);
				break;
			}
		}

		// Copy the actual data in.
		MmCopyFromMdl(MdlBuffer, MdlOffset, BlockBuffer + BlockOffset, CopyAmount);

		// Finally, write the block.
		Status = IoWriteFileMdl(
			Iosb,             // Iosb
			FileSystem->File, // FileObject
			BlockBufferMdl,   // Mdl
			0,                // Flags
			Address,          // FileOffset
			false             // Cached
		);

		if (FAILED(Status)) {
			ReleaseBlockRwlock(Ext);
			break;
		}

		ReleaseBlockRwlock(Ext);

		MdlOffset += CopyAmount;
		Offset += CopyAmount;
		BytesRead += CopyAmount;
		Size -= CopyAmount;
	}

	if (UsingTempBlockSizeBuffer)
	{
		ExReleaseFastMutex(&FileSystem->TempBlockSizeBufferMutex);
	}
	else
	{
		MmFreeMdl(BlockBufferMdl);
		MmFreePool(BlockBuffer);
	}

	Iosb->BytesRead = BytesRead;
	Iosb->Status = Status;

	MmFreePool(BlockBuffer);
	return Status;
}
