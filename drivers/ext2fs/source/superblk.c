/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	superblk.c

Abstract:
	This module manages the Ext2 super block.

Author:
	iProgramInCpp - 3 November 2026
***/
#include "ext2.h"

void Ext2IncrementFreeBlocksSuperBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t AmountBy
)
{
	KIPL Ipl;
	KeAcquireSpinLock(&FileSystem->SuperBlockSpinLock, &Ipl);

	FileSystem->SuperBlockIsDirty = true;
	uint32_t NewBlockCount = FileSystem->SuperBlock.FreeBlockCount;

	if (NewBlockCount + AmountBy >= NewBlockCount)
	{
		NewBlockCount += AmountBy;
	}
	else
	{
		DbgPrint(
			"Ext2: Accounting error: There are supposedly %u blocks free, and we're trying to "
			"claim %u. That would overflow the free block counter. Run 'fsck' on your ext2 partition.",
			NewBlockCount,
			AmountBy
		);
		NewBlockCount = 0xFFFFFFFF;
	}

	FileSystem->SuperBlock.FreeBlockCount = NewBlockCount;

	KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
}

bool Ext2DecrementFreeBlocksSuperBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	uint32_t AmountBy
)
{
	KIPL Ipl;
	KeAcquireSpinLock(&FileSystem->SuperBlockSpinLock, &Ipl);

	uint32_t NewBlockCount = FileSystem->SuperBlock.FreeBlockCount;

	if (AmountBy > NewBlockCount)
	{
		DbgPrint(
			"Ext2: Out of space.  Trying to allocate %u blocks, but we only have %u free.",
			AmountBy,
			NewBlockCount
		);
		KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
		return false;
	}
	else
	{
		FileSystem->SuperBlock.FreeBlockCount = NewBlockCount - AmountBy;
		FileSystem->SuperBlockIsDirty = true;
	}

	KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
	return true;
}

void Ext2ModifyFreeInodesSuperBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	bool Decrement
)
{
	KIPL Ipl;
	KeAcquireSpinLock(&FileSystem->SuperBlockSpinLock, &Ipl);

	FileSystem->SuperBlockIsDirty = true;

	if (Decrement)
	{
		if (FileSystem->SuperBlock.FreeInodeCount == 0)
		{
			DbgPrint(
				"Ext2: Accounting error: There are supposedly %u inodes free, but we're trying to "
				"allocate one.  Run 'fsck' on your ext2 partition.",
				FileSystem->SuperBlock.FreeInodeCount
			);
		}
		else
		{
			FileSystem->SuperBlock.FreeInodeCount--;
		}
	}
	else
	{
		FileSystem->SuperBlock.FreeInodeCount++;
	}

	KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
}

//
// Gets a snapshot of the superblock, for writeback, and optionally clears the dirty flag.
//
bool Ext2GetSuperBlockSnapshot(
	PEXT2_FILE_SYSTEM FileSystem,
	PEXT2_SUPERBLOCK OutSuperblock,
	bool ClearDirty,
	bool CopyEvenIfClean
)
{
	KIPL Ipl;
	KeAcquireSpinLock(&FileSystem->SuperBlockSpinLock, &Ipl);

	bool WasDirty = FileSystem->SuperBlockIsDirty;
	if (WasDirty || CopyEvenIfClean) {
		memcpy(OutSuperblock, &FileSystem->SuperBlock, sizeof(EXT2_SUPERBLOCK));
	}

	if (ClearDirty) {
		FileSystem->SuperBlockIsDirty = false;
	}

	KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
	return WasDirty;
}
