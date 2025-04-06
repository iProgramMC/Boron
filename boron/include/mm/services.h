/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/services.h
	
Abstract:
	This header defines the Boron memory manager's
	exposed system services.
	
Author:
	iProgramInCpp - 13 March 2025
***/
#pragma once

#include <status.h>

// Allocation Types
enum
{
	MEM_RESERVE  = 0x0001, // The memory range will be reserved.
	MEM_COMMIT   = 0x0002, // The memory range will be committed.
	MEM_SHARED   = 0x0004, // The memory range will be shared and will be passed down in forks.
	MEM_TOP_DOWN = 0x0008, // Addresses near the top of the user address space are preferred.
	MEM_DECOMMIT = 0x0010, // The memory range will be decommitted.
	MEM_RELEASE  = 0x0020, // The memory range will be released.
	MEM_COW      = 0x0040, // The memory will be copied on write, instead of any writes being committed to the backing file.
};

BSTATUS OSAllocateVirtualMemory(
	void** BaseAddressInOut,
	size_t* RegionSizeInOut,
	int AllocationType,
	int Protection
);

BSTATUS OSFreeVirtualMemory(
	void* BaseAddress,
	size_t RegionSize,
	int FreeType
);
