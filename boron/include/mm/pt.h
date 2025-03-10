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
//
// Note: If you *must* hold the VAD list lock, always acquire the space lock first to avoid deadlock.
//
// TODO: The space lock should be superseded by the VAD list lock, and the latter should be the only
// thing responsible for userspace memory management, or kernel side memory management outside of
// pool space. (update 9/3/25: should we?!  This rwlock guards the page table structures in general)
//
// NOTE: Raises the IPL to IPL_APC level.  This is because APCs may also take page faults.  We don't
// want a situation where the page fault handler is interrupted by an APC *while* it is handling
// a page fault and modifying the current process' memory management structures.
NO_DISCARD KIPL MmLockSpaceShared(uintptr_t DecidingAddress);
NO_DISCARD KIPL MmLockSpaceExclusive(uintptr_t DecidingAddress);
void MmUnlockSpace(KIPL OldIpl, uintptr_t DecidingAddress);

// Gets the current page mapping.
HPAGEMAP MiGetCurrentPageMap();

// Creates a page mapping.
HPAGEMAP MiCreatePageMapping(HPAGEMAP OldPageMapping);

// Deletes a page mapping.
void MiFreePageMapping(HPAGEMAP OldPageMapping);

// Resolves a page table entry pointer (virtual address offset by HHDM) relative to an address.
// Can allocate the missing page mapping levels on its way if the flag is set.
// If on its way, it hits a higher page size, currently it will return null since it's not really
// designed for that.
PMMPTE MiGetPTEPointer(HPAGEMAP Mapping, uintptr_t Address, bool AllocateMissingPMLs);

// Gets the PTE's location in the recursive PTE. This doesn't care whether the PTEs
// actually exist down to that address. Also this works with higher page mapping levels too.
PMMPTE MmGetPteLocation(uintptr_t Address);

// Check if the PTE for a certain VA exists at the recursive PTE address, in the
// current page mapping.
//
// If it doesn't, and GenerateMissingLevels is true, then this function attempts
// to construct the page tables up to that point.
//
// NOTE: This does NOT work with addresses within the recursive PTE range itself.
//
// Returns false if:
//   If GenerateMissingLevels is true, then the system ran out of memory trying to
//   generate the missing levels and memory should be freed.
//   If GenerateMissingLevels is false, then the PTE location is currently
//   inaccessible and should be regenerated.
//
// Returns true if the PTE location is accessible after the call (the PTEs may have
// been generated if GenerateMissingLevels is true).
bool MmCheckPteLocation(uintptr_t Address, bool GenerateMissingLevels);

// Attempts to map a physical page into the specified address space.
bool MiMapAnonPage(HPAGEMAP Mapping, uintptr_t Address, uintptr_t Permissions, bool NonPaged);

// Attempts to map several anonymous pages into the specified address space.
bool MiMapAnonPages(HPAGEMAP Mapping, uintptr_t Address, size_t SizePages, uintptr_t Permissions, bool NonPaged);

// Attempts to map a known physical page into the specified address space.
bool MiMapPhysicalPage(HPAGEMAP Mapping, uintptr_t PhysicalPage, uintptr_t Address, uintptr_t Permissions);

// Unmaps some memory. Automatically frees it if it is handled by the PMM.
void MiUnmapPages(HPAGEMAP Mapping, uintptr_t Address, size_t LengthPages); 

// Handles a page fault. Returns whether or not the page fault was handled.
// TODO make it MiPageFault and export it only to ke/except
BSTATUS MmPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode);

// Issue a TLB shootdown request. This is the official API for this purpose.
void MmIssueTLBShootDown(uintptr_t Address, size_t LengthPages);

// Turn access flags (PAGE_X) into PTE protection bits.
MMPTE MmGetPteBitsFromProtection(int Protection);

#endif//BORON_MM_PT_H
