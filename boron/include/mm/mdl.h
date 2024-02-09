/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/mdl.h
	
Abstract:
	This header file defines the MDL (memory descriptor list)
	structure.
	
Author:
	iProgramInCpp - 21 January 2024
***/
#pragma once

#include <mm/pfn.h>
#include <status.h>

// XXX: This is completely arbitrary.
#define MDL_MAX_SIZE (4*1024*1024)

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

typedef struct _MDL
{
	short ByteOffset;            // Offset within the first captured page
	short Flags;                 // Padding for now
	uint32_t Available;          // Padding
	size_t ByteCount;            // Direct size from MmCaptureMdl
	uintptr_t SourceStartVA;     // The virtual address where this MDL was captured from
	uintptr_t MappedStartVA;     // The virtual address where this MDL is mapped into system memory
	BIG_MEMORY_HANDLE MapHandle; // The handle to the pool space area where the memory was mapped
	PEPROCESS Process;           // Process these pages belong to
	size_t NumberPages;          // Size of the page frame number list
	int Pages[];
}
MDL, *PMDL;

// Initializes and captures an MDL from a virtual address.
// The virtual address must be located in the user half.
BSTATUS MmCaptureMdl(PMDL* Mdl, uintptr_t VirtualAddress, size_t Size);

// Deallocates an MDL and releases reference to all pages.
// The MDL must not be mapped into kernel memory.
void MmFreeMDL(PMDL Mdl);

// Maps an MDL into pool space. Use MmUnmapMDL to unmap the MDL.
BSTATUS MmMapMDL(PMDL Mdl, uintptr_t* OutAddress, uintptr_t Permissions);

// Unmaps an MDL from pool space.
BSTATUS MmUnmapMDL(PMDL Mdl);
