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
	// The memory range will be reserved, so it won't be allocated again.
	MEM_RESERVE  = 0x0001,
	
	// The memory range will be committed.  It must have been reserved first.
	MEM_COMMIT   = 0x0002,
	
	// The memory range will be shared and will be passed down as-is in duplicated
	// processes. If this flag isn't set, then when the process is duplicated, the
	// memory range will be duplicated with copy-on-write.
	MEM_SHARED   = 0x0004, 
	
	// This is a hint for the allocator to place the memory range near the top of
	// the user address space.
	MEM_TOP_DOWN = 0x0008,
	
	// The memory range will be decommitted, but not released.
	// Note that for releasing you must only set MEM_RELEASE, *not* the combination
	// MEM_DECOMMIT | MEM_RELEASE, which is deemed invalid by the operating system.
	MEM_DECOMMIT = 0x0010,
	
	// The memory range will be decommitted (if it was previously committed) and released.
	MEM_RELEASE  = 0x0020,
	
	// The file-backed mapped memory allows writing, but any writes to the memory
	// will not be committed to the backing file.
	MEM_COW      = 0x0040,
	
	// The requested memory mapping operation must be performed at the specified range.
	// By default, STATUS_CONFLICTING_ADDRESSES will be returned if the specified range
	// overlaps any existing ranges of memory.
	MEM_FIXED    = 0x0080,
	
	// This flag must be provided alongside MEM_FIXED.  Instead of returning the status
	// STATUS_CONFLICTING_ADDRESSES, the existing memory ranges will be replaced with the
	// requested memory map.
	MEM_OVERRIDE = 0x0100,
	
	// This flag must be provided alongside MEM_RELEASE.  Instead of returning the status
	// STATUS_CONFLICTING_ADDRESSES, any overlapping memory ranges will be shrunk, or 
	// removed entirely.
	MEM_PARTIAL  = 0x0200,
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
