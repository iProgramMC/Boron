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
