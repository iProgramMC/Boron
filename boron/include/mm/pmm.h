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

// Initialize the physical memory manager.
void MiInitPMM();

// Get the base address of the HHDM (higher half direct map)
uint8_t* MmGetHHDMBase();

// Converts a physical address into an HHDM address.
void* MmGetHHDMOffsetAddr(uintptr_t physAddr);

// Converts an HHDM based address into a physical address.
uintptr_t MmGetHHDMOffsetFromAddr(void* addr);

// Converts a physical address to a page frame number (PFN).
int MmPhysPageToPFN(uintptr_t physAddr);

// Converts a page frame number (PFN) to a physical page.
uintptr_t MmPFNToPhysPage(int pfn);

// This returns a PFN, use MmPFNToPhysPage to convert it to
// a physical address, and MmGetHHDMOffsetAddr to address into it directly
int MmAllocatePhysicalPage();

// Adds a reference to a PFN.
void MmPageAddReference(int Pfn);

// This expects a PFN. Use MmPhysPageToPFN if you have a physically
// addressed page to free. Lowers the reference counter of the physical page.
// When zero, the page is freed from physical memory and can be used again.
void MmFreePhysicalPage(int pfn);

// Same as MmAllocatePhysicalPage, but returns an HHDM based address.
void* MmAllocatePhysicalPageHHDM();

// Same as MmFreePhysicalPage, but takes in an address from MmAllocatePhysicalPageHHDM
// instead of a PFN.
void MmFreePhysicalPageHHDM(void* page);

// Gets the total amount of free pages.
size_t MmGetTotalFreePages();

#endif//BORON_MM_PMM_H
