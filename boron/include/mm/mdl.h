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
#include <ke/mode.h>

// XXX: This is completely arbitrary.
#define MDL_MAX_SIZE (4*1024*1024)

// If set, the MDL's Pages[] array has been filled in.
#define MDL_FLAG_CAPTURED (1 << 0)

// If set, the MDL is mapped into kernel memory.
#define MDL_FLAG_MAPPED   (1 << 1)

// If set, the MDL reflects a write operation.
#define MDL_FLAG_WRITE    (1 << 2)

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

typedef struct _MDL
{
	short ByteOffset;            // Offset within the first captured page
	short Flags;                 // Padding for now
	uint32_t Available;          // Padding
	size_t ByteCount;            // Direct size from MmCaptureMdl
	uintptr_t SourceStartVA;     // The virtual address where this MDL was captured from
	uintptr_t MappedStartVA;     // The virtual address where this MDL is mapped into system memory
	PEPROCESS Process;           // Process these pages belong to
	size_t NumberPages;          // Size of the page frame number list
	int Pages[];
}
MDL, *PMDL;

// Allocates an MDL structure.
PMDL MmAllocateMdl(uintptr_t VirtualAddress, size_t Length);

// Probes the given virtual address and tries to pin all the buffer's pages.
BSTATUS MmProbeAndPinPagesMdl(PMDL Mdl, KPROCESSOR_MODE AccessMode, bool IsWrite);

// Unpins the buffer's pages and releases the reference to them.
void MmUnpinPagesMdl(PMDL Mdl);

// Maps an MDL into pool space. Use MmUnmapPinnedPagesMdl to unmap the MDL.
//
// If the MDL is already mapped, the address of the already existing mapping
// will simply be stored in OutAddress and a status code of STATUS_NO_REMAP
// will be returned.
//
// The OutAddress specified will be offset by the page offset specified
// at allocation time.
BSTATUS MmMapPinnedPagesMdl(PMDL Mdl, void** OutAddress);

// Unmaps an MDL from pool space.
void MmUnmapPagesMdl(PMDL Mdl);

// Deallocates the MDL structure, unmaps the MDL, and unpins the buffer.
void MmFreeMdl(PMDL Mdl);