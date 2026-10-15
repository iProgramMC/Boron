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

// Flushes the page cache of a file object.
BSTATUS MmFlushFileCache(PFILE_OBJECT FileObject);

// Partially flushes the page cache of a file object.
BSTATUS MmFlushFileCacheEx(PFILE_OBJECT FileObject, uint64_t StartOffset, size_t SizeBytes);
