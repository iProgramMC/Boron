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

#ifdef IS_64_BIT

typedef uintptr_t *MM_PROTOTYPE_PTE_PTR, **PMM_PROTOTYPE_PTE_PTR;
#define MM_PROTOTYPE_PTE_PTR_NONE (NULL)

#else

// 32-bit prototype PTE addresses should be OR'd with this value to
// make them virtual.
#define MM_PROTO_PTE_PTR_IS_VIRTUAL (1 << 0)
#define MM_VIRTUAL_PROTO_PTE_PTR(Ptr) ((uintptr_t)(Ptr) | 1)
#define MM_PROTOTYPE_PTE_PTR_NONE (0)
typedef uintptr_t MM_PROTOTYPE_PTE_PTR, *PMM_PROTOTYPE_PTE_PTR;

#endif

typedef struct _FCB FCB, *PFCB;

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

#define MmGetHHDMOffsetAddrPfn(Pfn) MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn))

#ifdef IS_32_BIT

void MmBeginUsingHHDM(void);
void MmEndUsingHHDM(void);

#else

#define MmBeginUsingHHDM()
#define MmEndUsingHHDM()

#endif

// Converts a page frame number (PFN) to a physical page.
uintptr_t MmPFNToPhysPage(MMPFN Pfn);

// This returns a PFN, use MmPFNToPhysPage to convert it to
// a physical address, and MmGetHHDMOffsetAddr to address into it directly
MMPFN MmAllocatePhysicalPage(void);

// Adds a reference to a PFN.
void MmPageAddReference(MMPFN Pfn);

// Sets a referenced PFN as modified.
void MmSetModifiedPage(MMPFN Pfn);

// Assign a prototype PTE address to the page frame.
void MmSetPrototypePtePfn(MMPFN Pfn, MM_PROTOTYPE_PTE_PTR PrototypePte);

// Assign an FCB pointer and offset, to the page frame.
//
// Note that the reference to the FCB is weak, i.e. it does not count towards
// the FCB's reference count.  When the FCB is deleted, the entire page cache
// is deleted anyway, letting go all of the pages.
//
// The offset is saved in multiples of page size, but the passed in offset
// is in bytes.
void MmSetCacheDetailsPfn(MMPFN Pfn, PFCB Fcb, uint64_t Offset);

// Set an allocated page as modified.
void MmSetModifiedPfn(MMPFN Pfn);

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

// Registers a range of memory as MMIO.  This means that user applications
// will be able to map this memory in directly, however, the pages will
// never behave like regular memory.
void MmRegisterMMIOAsMemory(uintptr_t Base, uintptr_t Size);

// TODO: Not sure where to place this.  Do we really need a new file?
void MmInitializeModifiedPageWriter(void);

// Allocates a contiguous memory region with an address alignment.  Note that
// this is slow and so it should be called cautiously.  Thanks for your stupid
// unconventional page table layout, ARM!  Someday I may opt to implement a
// buddy system, but not as of now.
MMPFN MmAllocatePhysicalContiguousRegion(int PageCount, uintptr_t Alignment);

// Frees a physical contiguous region.  It basically frees every PFN between
// PfnStart and PfnStart + PageCount - 1.
void MmFreePhysicalContiguousRegion(MMPFN PfnStart, int PageCount);

#endif//BORON_MM_PMM_H
