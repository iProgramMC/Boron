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

#include <ke/ipl.h>
#include <ke/locks.h>

// handle to a page mapping.
typedef uintptr_t HPAGEMAP;

// WARNING! The HPAGEMAP object is not thread safe! So please take care of thread safety yourself.
// Ideally the address space of a process will be managed by some other structure.

// Acquires the lock that guards the kernel's half of the address space.
void MmLockKernelSpaceShared();
void MmLockKernelSpaceExclusive();

// Releases the lock that guards the kernel's half of the address space.
void MmUnlockKernelSpace();

// Locks either the user space lock or the kernel space lock, depending on the address.
// Pass 0 to lock the user space lock.
void MmLockSpaceShared(uintptr_t DecidingAddress);
void MmLockSpaceExclusive(uintptr_t DecidingAddress);
void MmUnlockSpace(uintptr_t DecidingAddress);

// Gets the current page mapping.
HPAGEMAP MiGetCurrentPageMap();

// Creates a page mapping.
HPAGEMAP MiCreatePageMapping(HPAGEMAP OldPageMapping);

// Deletes a page mapping.
void MiFreePageMapping(HPAGEMAP OldPageMapping);

// Resolves a page table entry pointer (virtual address offset by HHDM) relative to an address.
// Can allocate the missing page mapping levels on its way if the flag is set.
// If on its way, it hits a higher page size, currently it will return null since it's not really
// designed for that..
PMMPTE MiGetPTEPointer(HPAGEMAP Mapping, uintptr_t Address, bool AllocateMissingPMLs);

// Attempts to map a physical page into the specified address space.
bool MiMapAnonPage(HPAGEMAP Mapping, uintptr_t Address, uintptr_t Permissions, bool NonPaged);

// Attempts to map several anonymous pages into the specified address space.
bool MiMapAnonPages(HPAGEMAP Mapping, uintptr_t Address, size_t SizePages, uintptr_t Permissions, bool NonPaged);

// Attempts to map a known physical page into the specified address space.
bool MiMapPhysicalPage(HPAGEMAP Mapping, uintptr_t PhysicalPage, uintptr_t Address, uintptr_t Permissions);

// Unmaps some memory. Automatically frees it if it is handled by the PMM.
void MiUnmapPages(HPAGEMAP Mapping, uintptr_t Address, size_t LengthPages); 

// Handles a page fault. Returns whether or not the page fault was handled.
BSTATUS MmPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode);

// Issue a TLB shootdown request. This is the official API for this purpose.
void MmIssueTLBShootDown(uintptr_t Address, size_t LengthPages);

#endif//BORON_MM_PT_H
