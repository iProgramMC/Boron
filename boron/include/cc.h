/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	cc.h
	
Abstract:
	This header defines all of the structures necessary
	for the implementation of the Boron view cache manager.
	
Author:
	iProgramInCpp - 23 May 2025
***/
#pragma once

#include <main.h>

// The internal view cache's size.
#ifdef IS_64_BIT

// 16 MB on 64-bit.
#define VIEW_CACHE_SIZE (16 * 1024 * 1024)

// A maximum of 128 GB (8K * 16M) can be allocated to system space views.
#define VIEW_CACHE_MAX_COUNT (8192)

#else

// 128 KB on 32-bit.
#define VIEW_CACHE_SIZE (128 * 1024)

// A maximum of 128 MB (1K * 128K) can be allocated to system space views.
#define VIEW_CACHE_MAX_COUNT (1024)

#endif

typedef struct _MMVAD_ENTRY MMVAD, *PMMVAD;
typedef struct _FCB FCB, *PFCB;
typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;
typedef struct _MDL MDL, *PMDL;

// -- View Cache LRU List --

// Adds a VAD to the view cache LRU list.
void CcAddVadToViewCacheLru(PMMVAD Vad);

// Removes a VAD from the view cache LRU list.
void CcRemoveVadFromViewCacheLru(PMMVAD Vad);

// Moves a VAD to the front of the view cache LRU list.
// This is called when a system space VAD is faulted on.
void CcOnSystemSpaceVadUsed(PMMVAD Vad);

// Removes the head of the view cache LRU list for freeing.
// This is used if view space is running out.
PMMVAD CcRemoveHeadOfViewCacheLru();

// Purge any VADs that go over the limit plus a specified number.
void CcPurgeViewsOverLimit(int LeaveSpaceFor);

// Purge any VADs associated with this FCB.
// This function is to be called when the FCB loses its last reference.
void CcPurgeViewsForFile(PFCB Fcb);

// Reads the contents of a file and copies them to a buffer pinned by an MDL.
// The MDL's ByteCount member is ignored for this one.
BSTATUS CcReadFileMdl(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	PMDL Mdl,
	uintptr_t MdlOffset,
	size_t ByteCount
);

// Writes the contents of a buffer to a file through an MDL.
// The MDL's ByteCount member is ignored for this one.
BSTATUS CcWriteFileMdl(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	PMDL Mdl,
	uintptr_t MdlOffset,
	size_t ByteCount
);

// Reads the contents of a file and copies them to a buffer.
BSTATUS CcReadFileCopy(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	void* Buffer,
	size_t Size
);

// Reads the contents of a file and copies them to a buffer.
BSTATUS CcWriteFileCopy(
	PFILE_OBJECT FileObject,
	uint64_t FileOffset,
	const void* Buffer,
	size_t Size
);
