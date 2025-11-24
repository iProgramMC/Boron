/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mms.h
	
Abstract:
	This header file contains the publicly exposed structure
	definitions for Boron's Memory Subsystem.
	
Author:
	iProgramInCpp - 22 April 2025
***/
#pragma once

#include "handle.h"

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
	MEM_FIXED    = 0x0080, // The requested memory mapping operation must occur at the specified offset.
	MEM_OVERRIDE = 0x0100, // The requested memory mapping operation will override any extant allocations.
};

enum ACCESS_FLAG
{
	PAGE_READ    = 1,
	PAGE_WRITE   = 2,
	PAGE_EXECUTE = 4,
};

// Page Size definition
#if defined TARGET_AMD64 || defined TARGET_I386
#define PAGE_SIZE (0x1000)
#else
#error Define page size here!
#endif
