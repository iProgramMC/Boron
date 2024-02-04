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

// 4 MB. If you need a larger MDL try using several.
// I/O requests are probably going to be split up anyway.
// Conveniently the list of PFNs will always occupy only one page.
#define MDL_MAX_SIZE (4*1024*1024)

typedef struct EPROCESS_tag EPROCESS, *PEPROCESS;

typedef struct _MDL
{
	short ByteOffset;        // Offset within the first captured page
	short Flags;             // Padding for now
	uint32_t Available;      // Padding
	size_t ByteCount;        // Direct size from MmCaptureMdl
	uintptr_t MappedStartVA; // The beginning of the virtual address where this MDL was captured from
	PEPROCESS Process;       // Process these pages belong to
	size_t NumberPages;      // Size of the page frame number list
	int Pages[];
}
MDL, *PMDL;

#define MDL_FIELD_INITIALIZED 

// Initializes and captures an MDL from a virtual address.
// The virtual address must be located in the user half.
BSTATUS MmCaptureMdl(PMDL* Mdl, uintptr_t VirtualAddress, size_t Size);
