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

// TODO: This is completely arbitrary.
// Consider allowing a bigger size.
#define MDL_MAX_SIZE (4*1024*1024)

// If set, the MDL's Pages[] array has been filled in.
#define MDL_FLAG_CAPTURED (1 << 0)

// If set, the MDL is mapped into kernel memory.
#define MDL_FLAG_MAPPED   (1 << 1)

// If set, the MDL reflects a write operation.
#define MDL_FLAG_WRITE    (1 << 2)

// If set, the MDL is allocated from the kernel pool.
#define MDL_FLAG_FROMPOOL (1 << 3)

// If set, the MDL is allocated from a pre-allocated pool of MDLs.
//#define MDL_FLAG_PREALLOC (1 << 4)

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
	MMPFN Pages[];
}
MDL, *PMDL;

typedef struct _MDL_ONEPAGE
{
	MDL Base;
	MMPFN Pages[1];
}
MDL_ONEPAGE, *PMDL_ONEPAGE;

// Initializes an MDL structure with one physical page.
// Returns the pointer to the MDL instance that was initialized.
//
// Do not use MDL_FLAG_FROMPOOL or MDL_FLAG_MAPPED.
PMDL_ONEPAGE MmInitializeSinglePageMdl(PMDL_ONEPAGE Mdl, MMPFN Pfn, int Flags);

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

// Unmaps the MDL, and unpins the buffer, and deallocates the MDL if needed.
void MmFreeMdl(PMDL Mdl);