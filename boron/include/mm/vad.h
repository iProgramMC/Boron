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

#define MM_VAD_MUTEX_LEVEL (3)
#define MM_KERNEL_SPACE_MUTEX_LEVEL (3) // TODO: not sure where it is appropriate to define this

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

typedef union
{
	struct
	{
		// Whether this VAD's default state is "committed".
		// If true, any page fault inside this range refers to a "committed"
		// address. Otherwise, the PTEs have to be specifically marked "committed"
		// to be committed.
		unsigned Committed : 1;
		
		// Protection flags, see the ACCESS_FLAG enum.
		unsigned Protection : 3;
		
		// If this region is private (so, copy-on-write duplicated across forks for example)
		unsigned Private : 1;
		
		// If this region is marked as "copy on write"
		unsigned Cow : 1;
	};
	
	uint32_t LongFlags;
}
MMVAD_FLAGS;

typedef struct _MMVAD_ENTRY
{
	MMADDRESS_NODE Node;
	
	MMVAD_FLAGS Flags;
	
	// Note: Ref count is increased by 1 because of this reference.
	void* MappedObject;
	
	// If this is a file or section, then the offset within it.
	uint64_t SectionOffset;
	
	union
	{
		RBTREE_ENTRY ViewCacheEntry;
		
		struct
		{
			uintptr_t Spare1;
			uintptr_t Spare2;
			uintptr_t Spare3;
			uintptr_t Spare4;
		};
	};
	
	union
	{
		LIST_ENTRY ViewCacheLruEntry;
		
		struct
		{
			uintptr_t Spare5;
			uintptr_t Spare6;
		};
	};
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
#define MmLockVadList()  MmLockVadListProcess(PsGetAttachedProcess())

#define MmGetVadListProcess()  (&PsGetAttachedProcess()->VadList)

// Unlock the VAD list specified.
void MmUnlockVadList(PMMVAD_LIST);

// Looks up a VAD by address in a VAD list previously locked with MmLockVadListProcess.
PMMVAD MmLookUpVadByAddress(PMMVAD_LIST VadList, uintptr_t Address);

// Reserves a range of virtual memory and returns an address.
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

// Tear down the VAD and heap of a process.
void MmTearDownProcess(PEPROCESS Process);
