/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/mpw.h

Abstract:
	This header defines the interface with the Modified Page Writer (MPW).

Author:
	iProgramInCpp - 29 March 2026
***/
#pragma once

void MmModifiedPageWriterShutDown();

void MmStartFlushingModifiedPages();

typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;

// Flushes the page cache of an FCB to its backing store.
BSTATUS MmFlushFileCache(PFCB Fcb);

// Partially flushes the page cache of an FCB to its backing store.
BSTATUS MmFlushFileCacheEx(PFCB Fcb, uint64_t StartOffset, size_t SizeBytes);

// Truncates the file cache associated with an FCB, dropping all pages and
// (for now) turning them into anonymous memory.  Does not flush modified pages
// back to their backing store.
void MmTruncateFileCache(PFCB Fcb, uint64_t StartOffset);
