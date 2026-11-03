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
#include <string.h>

void Ext2IncrementAllocatedBlocksSuperBlock(
	PEXT2_FILE_SYSTEM FileSystem,
	int32_t AmountBy
)
{
	KIPL Ipl;
	KeAcquireSpinLock(&FileSystem->SuperBlockSpinLock, &Ipl);

	FileSystem->SuperBlockIsDirty = true;
	uint32_t NewBlockCount = FileSystem->SuperBlock.FreeBlockCount;

	if (AmountBy < 0)
	{
		if (NewBlockCount < 2147483647 && -AmountBy > (int32_t) NewBlockCount)
		{
			DbgPrint(
				"Ext2: Accounting error: There are supposedly %u blocks free, but we're trying to "
				"claim %u.  Run 'fsck' on your ext2 partition.",
				NewBlockCount,
				(uint32_t) -AmountBy
			);
			NewBlockCount = 0;
		}
		else
		{
			NewBlockCount += AmountBy;
		}
	}
	else
	{
		ASSERT(NewBlockCount + AmountBy >= NewBlockCount && "Integer overflow");
		NewBlockCount += AmountBy;
	}

	FileSystem->SuperBlock.FreeBlockCount = NewBlockCount;

	KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
}

void Ext2ModifyAllocatedInodesSuperBlock(
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
	if (CopyEvenIfClean) {
		memcpy(OutSuperblock, &FileSystem->SuperBlock, sizeof(EXT2_SUPERBLOCK));
	}

	if (ClearDirty) {
		FileSystem->SuperBlockIsDirty = false;
	}

	KeReleaseSpinLock(&FileSystem->SuperBlockSpinLock, Ipl);
	return WasDirty;
}
