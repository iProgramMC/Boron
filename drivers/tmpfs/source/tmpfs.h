/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	tmpfs.h
	
Abstract:
	This header file defines structs and function prototypes
	pertaining to the temporary file system driver.
	
Author:
	iProgramInCpp - 3 November 2025
***/
#pragma once

#include <ke.h>
#include <io.h>
#include <string.h>

#define IOSB_STATUS(Iosb, TheStatus) ((Iosb)->Status = (TheStatus))

typedef struct _TMPFS_DIR_ENTRY TMPFS_DIR_ENTRY, *PTMPFS_DIR_ENTRY;

// NOTE: Currently supported file types are FILE_TYPE_FILE, FILE_TYPE_DIRECTORY
// and FILE_TYPE_SYMBOLIC_LINK.
typedef struct
{
	KMUTEX Lock;
	
	// The amount of references to this tmpfs file.  This allows hard
	// links to work.
	size_t ReferenceCount;
	
	void* InitialAddress;
	size_t InitialLength;
	
	uint64_t InodeNumber;
	
	// The list of pages that define this file.
	//
	// NOTE: This is inactive for directory tmpfs files.
	PMMPFN PageList;
	size_t PageListCount;
	
	// The list of children. Active if this is a directory.
	LIST_ENTRY ChildrenListHead;
	
	// The current version of the file.  Active if this is a directory.
	uint64_t Version;
	
	// If this is a directory.
	PTMPFS_DIR_ENTRY ParentEntry;
}
TMPFS_EXT, *PTMPFS_EXT;

struct _TMPFS_DIR_ENTRY
{
	LIST_ENTRY ListEntry;
	
	PFCB ParentFcb;
	PFCB Fcb;
	char Name[IO_MAX_NAME];
};

BSTATUS TmpMountTarFile(PLOADER_MODULE Module);

BSTATUS TmpCreateFile(PFILE_OBJECT* OutFileObject, void* InitialAddress, size_t InitialLength, uint8_t FileType, PFCB ParentFcb, const char* Name);
