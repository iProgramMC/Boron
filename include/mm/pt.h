/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pt.h
	
Abstract:
	This header file contains the definitions for the
	platform independent interface for page table management.
	
Author:
	iProgramInCpp - 28 September 2023
***/
#ifndef BORON_MM_PT_H
#define BORON_MM_PT_H

// handle to a page mapping.
typedef uintptr_t HPAGEMAP;

// List of reasons why a page fault can fail.
enum
{
	FAULT_HANDLED,
	FAULT_UNSUPPORTED,
	FAULT_UNMAPPED,
	FAULT_OUTOFMEMORY,
	FAULT_HIGHIPL,
};

// WARNING! The HPAGEMAP object is not thread safe! So please take care of thread safety yourself.
// Ideally the address space of a process will be managed by some other structure.

// Gets the current page mapping.
HPAGEMAP MmGetCurrentPageMap();

// Creates a page mapping.
HPAGEMAP MmCreatePageMapping(HPAGEMAP OldPageMapping);

// Deletes a page mapping.
void MmFreePageMapping(HPAGEMAP OldPageMapping);

// Resolves a page table entry pointer (virtual address offset by HHDM) relative to an address.
// Can allocate the missing page mapping levels on its way if the flag is set.
// If on its way, it hits a higher page size, currently it will return null since it's not really
// designed for that..
PMMPTE MmGetPTEPointer(HPAGEMAP Mapping, uintptr_t Address, bool AllocateMissingPMLs);

// Attempts to map a physical page into the specified address space.
bool MmMapAnonPage(HPAGEMAP Mapping, uintptr_t Address, uintptr_t Permissions, bool NonPaged);

// Attempts to map several anonymous pages into the specified address space.
bool MmMapAnonPages(HPAGEMAP Mapping, uintptr_t Address, size_t SizePages, uintptr_t Permissions, bool NonPaged);

// Attempts to map a known physical page into the specified address space.
bool MmMapPhysicalPage(HPAGEMAP Mapping, uintptr_t PhysicalPage, uintptr_t Address, uintptr_t Permissions);

// Unmaps some memory. Automatically frees it if it is handled by the PMM.
void MmUnmapPages(HPAGEMAP Mapping, uintptr_t Address, size_t LengthPages); 

// Handles a page fault. Returns whether or not the page fault was handled.
int MmPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode);

// Issue a TLB shootdown request. This is the official API for this purpose.
void MmIssueTLBShootDown(uintptr_t Address, size_t LengthPages);

#endif//BORON_MM_PT_H
