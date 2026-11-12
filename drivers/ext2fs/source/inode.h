/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	inode.h

Abstract:
	This header defines helpers for inode manipulation functions.

Author:
	iProgramInCpp - 9 November 2026
***/
#pragma once

// Prepare the extension.
#define PREP_EXT PEXT2_FCB_EXT Ext = EXT(Fcb)

#define AcquireInodeTreeMutex(fs) ExAcquireFastMutex(&(fs)->InodeTreeMutex, false)
#define ReleaseInodeTreeMutex(fs) ExReleaseFastMutex(&(fs)->InodeTreeMutex)

#define AcquireBlockRwlockShared(in)    ExAcquireSharedRwLock(&(in)->BlockRwlock, false, false, false)
#define AcquireBlockRwlockExclusive(in) ExAcquireExclusiveRwLock(&(in)->BlockRwlock, false, false)
#define DemoteBlockRwlockToShared(in)   ExDemoteToSharedRwLock(&(in)->BlockRwlock)
#define ReleaseBlockRwlock(in) ExReleaseRwLock(&(in)->BlockRwlock)

#define IOSB_STATUS(iosb, stat) (iosb->Status = stat)

FORCE_INLINE uint64_t Ext2FileSize(PFCB Fcb)
{
	PREP_EXT;

	if (Ext->OwnerFS->SuperBlock.Version == 0)
		return Ext->Inode.Size;
	else
		return Ext->Inode.Size | ((uint64_t) Ext->Inode.SizeUpper << 32);
}

BSTATUS Ext2FindOnDiskBlock(PFCB Fcb, uint32_t BlockIndex, uint32_t* OnDiskBlockOut, bool AllocateIfNeeded, bool* AllocatedBlock);

int Ext2TypeIndicatorToFileType(uint8_t TypeIndicator);

int Ext2InodeModeToFileType(uint16_t Mode);
