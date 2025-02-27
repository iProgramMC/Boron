/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/vad.h
	
Abstract:
	
	
Author:
	iProgramInCpp - 18 August 2024
***/
#pragma once

#include <main.h>
#include <io/fileobj.h>

#include <mm/addrnode.h>

#define MM_VAD_MUTEX_LEVEL (1)

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

enum
{
	ACCESS_FLAG_READ    = 1,
	ACCESS_FLAG_WRITE   = 2,
	ACCESS_FLAG_EXECUTE = 4,
};

typedef struct _MMVAD_FLAGS
{
	// Access flags, as defined in the above enum.
	int AccessFlags : 3;
	
	// Whether or not the current VAD entry is a file.
	bool IsFile : 1;
	
	// Whether or not the current VAD entry is a section.
	bool IsSection : 1;
}
MMVAD_FLAGS;

typedef struct _MMVAD_ENTRY
{
	MMADDRESS_NODE Node;
	
	MMVAD_FLAGS Flags;
	
	// Note: Ref count is increased by 1 because of this reference.
	// This is a PFILE_OBJECT if IsFile is true.
	union
	{
		void* Object;
		PFILE_OBJECT FileObject;
	}
	Mapped;
	
	// If this is a file, then the offset within the file.
	uint64_t OffsetInFile;
}
MMVAD_ENTRY, *PMMVAD_ENTRY;

typedef struct _MMVAD_LIST
{
	KMUTEX Mutex;
	RBTREE Tree;
}
MMVAD_LIST, *PMMVAD_LIST;

// Initialize the VAD list for a process.
void MmInitializeVadList(PMMVAD_LIST VadList);

// Lock the VAD list of a specified process and returns a pointer to it.
PMMVAD_LIST MmLockVadListProcess(PEPROCESS Process);

// Lock the VAD list of the current process and returns a pointer to it.
#define MmLockVadList()  MmLockVadList(PsGetCurrentProcess())

// Unlock the VAD list specified.
void MmUnlockVadList(PMMVAD_LIST);
