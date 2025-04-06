/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pmm.h
	
Abstract:
	This header file contains the definitions for the
	physical memory manager (PMM).
	
Author:
	iProgramInCpp - 28 September 2023
***/
#ifndef BORON_MM_PMM_H
#define BORON_MM_PMM_H

#include <mm/pfn.h>

#ifdef KERNEL

// Initialize the physical memory manager.
void MiInitPMM(void);

// Reclaims the init text section.  This may be performed only once.
void MiReclaimInitText();

#endif

// Get the base address of the HHDM (higher half direct map)
uint8_t* MmGetHHDMBase(void);

// Converts a physical address into an HHDM address.
void* MmGetHHDMOffsetAddr(uintptr_t PhysAddr);

// Converts an HHDM based address into a physical address.
uintptr_t MmGetHHDMOffsetFromAddr(void* Addr);

// Converts a physical address to a page frame number (PFN).
MMPFN MmPhysPageToPFN(uintptr_t PhysAddr);

// Converts a page frame number (PFN) to a physical page.
uintptr_t MmPFNToPhysPage(MMPFN Pfn);

// This returns a PFN, use MmPFNToPhysPage to convert it to
// a physical address, and MmGetHHDMOffsetAddr to address into it directly
MMPFN MmAllocatePhysicalPage(void);

// Adds a reference to a PFN.
void MmPageAddReference(MMPFN Pfn);

// Assign a prototype PTE address to the page frame.
void MmSetPrototypePtePfn(MMPFN Pfn, uintptr_t* PrototypePte);

// This expects a PFN. Use MmPhysPageToPFN if you have a physically
// addressed page to free. Decrements the reference counter of the physical page.
// When zero, the page is freed from physical memory and can be used again.
void MmFreePhysicalPage(MMPFN Pfn);

// Same as MmAllocatePhysicalPage, but returns an HHDM based address.
void* MmAllocatePhysicalPageHHDM(void);

// Same as MmFreePhysicalPage, but takes in an address from MmAllocatePhysicalPageHHDM
// instead of a PFN.
void MmFreePhysicalPageHHDM(void* Page);

// Gets the total amount of free pages.
size_t MmGetTotalFreePages();

#endif//BORON_MM_PMM_H
