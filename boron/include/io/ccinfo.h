/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	io/ccinfo.h

Abstract:
	This header defines the I/O Cache Info structure.
	Not to be confused with the Cache Control Block
	structure from the memory manager.  This structure
	contains the View Cache tree, the Page Cache tree,
	as well as controlling when the Lazy Writer kicks in.

Author:
	iProgramInCpp - 12 August 2026
***/
#pragma once

#include <ex/rwlock.h>
#include <mm/cache.h>
#include <io/dispatch.h>

typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;

typedef struct
{
	KMUTEX ViewCacheMutex;
	RBTREE ViewCache;

	CCB PageCache;

	// If block size is not zero, the modification bitmap is active and
	// tracks page modification separately from the page cache.  (Also
	// means that pages won't get flushed to the backing store in the
	// usual manner)
	size_t BlockSize;
	KMUTEX ModBitmapMutex;
	PMMSLA ModBitmap;
}
IO_CACHE_INFO, *PIO_CACHE_INFO;

void IoInitializeCacheInfo(PIO_CACHE_INFO CacheInfo);

//
// Tear down an instance of IO_CACHE_INFO.
//
void IoTeardownCacheInfo(PIO_CACHE_INFO CacheInfo);

//
// Sets the current block size for a cache info structure.
//
// Set it to 0 to remove the modification bitmap, or set it to
// a different value to re-initialize it.
//
bool IoSetBlockSizeCacheInfo(PIO_CACHE_INFO CacheInfo, size_t BlockSize);

//
// Calls IoSetBlockSizeCacheInfo on the specified file object's cache info.
//
bool IoSetBlockSizeFile(PFILE_OBJECT FileObject, size_t BlockSize);

//
// Sets a range of blocks as modified in the cache info.
//
// Note: StartOffset and Size in # blocks.
//
bool IoSetRangeModifiedCacheInfo(PIO_CACHE_INFO CacheInfo, uint64_t StartOffset, size_t Size);
