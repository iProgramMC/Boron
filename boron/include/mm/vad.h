/***
	The Boron Operating System
	Copyright (C) 2024-2025 iProgramInCpp

Module name:
	mm/vad.h
	
Abstract:
	This header defines function prototypes related to the VAD
	(Virtual Address Descriptor) and its list.
	
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
	PAGE_READ    = 1,
	PAGE_WRITE   = 2,
	PAGE_EXECUTE = 4,
};

typedef union
{
	struct
	{
		// Whether this VAD's default state is "committed".
		// If true, any page fault inside this range refers to a "committed"
		// address. Otherwise, the PTEs have to be specifically marked "committed"
		// to be committed.
		int Committed : 1;
		
		// Protection flags, see the ACCESS_FLAG enum.
		int Protection : 3;
		
		// If this region is private (so, not duplicated across forks for example)
		int Private : 1;
	};
	
	uint32_t LongFlags;
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
MMVAD, *PMMVAD;

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
#define MmLockVadList()  MmLockVadListProcess(PsGetCurrentProcess())

// Unlock the VAD list specified.
void MmUnlockVadList(PMMVAD_LIST);

// Looks up a VAD by address in a VAD list previously locked with MmLockVadListProcess.
PMMVAD MmLookUpVadByAddress(PMMVAD_LIST VadList, uintptr_t Address);

// Reserves a bunch of virtual memory and returns an address.
BSTATUS MmReserveVirtualMemory(size_t SizePages, void** OutAddress, int AllocationType, int Protection);

// Releases a region of virtual memory.
BSTATUS MmReleaseVirtualMemory(void* Address);

// Commits a range of virtual memory that has been previously reserved.
//
// If Protection is 0, then it uses the reserved memory's own protection (MmReserveVirtualMemory).
BSTATUS MmCommitVirtualMemory(uintptr_t StartVa, size_t SizePages, int Protection);

// Decommits a range of virtual memory that has been previously reserved.
BSTATUS MmDecommitVirtualMemory(uintptr_t StartVa, size_t SizePages);

// Debug the VAD.
void MmDebugDumpVad();
