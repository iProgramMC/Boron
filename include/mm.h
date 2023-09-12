// Boron - Memory Manager
#ifndef NS64_MM_H
#define NS64_MM_H

#include <main.h>
#include <arch.h>

// handle to a page mapping.
typedef uintptr_t PageMapping;

// Page frame item in the page frame database.
// Keep this a power of 2, please.
typedef struct
{
	// Flags for the specifically referenced page frame.
	int m_flags;
	
	// Disregard if this is allocated. Eventually these will be part of a union
	// where they will take on different roles depending on the role of the page.
	int m_nextFrame;
	int m_prevFrame;
	
	// Disregard if this is free. Eventually this will be part of a union
	// where it will take on different roles depending on the role of the
	// page.
	int m_refCount;
}
PageFrame;

// page frame flags.
#define PF_FLAG_ALLOCATED (1 << 0)

#define PFN_INVALID (-1)

#define PFNS_PER_PAGE (PAGE_SIZE / sizeof(PageFrame))

typedef struct
{
	uint64_t entries[512];
}
PageMapLevel;

#define PTE_ADDRESS(pte) ((pte) & MM_PTE_ADDRESSMASK)

// ==== PMM ====

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

// this returns a PFN, use MmPFNToPhysPage to convert it to
// a physical address, and MmGetHHDMOffsetAddr to address into it directly
int MmAllocatePhysicalPage();

// this expects a PFN. Use MmPhysPageToPFN if you have a physically
// addressed page to free.
void MmFreePhysicalPage(int pfn);

// Same as MmAllocatePhysicalPage, but returns an HHDM based address.
void* MmAllocatePhysicalPageHHDM();

// Same as MmFreePhysicalPage, but takes in an address from MmAllocatePhysicalPageHHDM
// instead of a PFN.
void MmFreePhysicalPageHHDM(void* page);

// ==== VMM ====

// Creates a page mapping.
PageMapping MmCreatePageMapping(PageMapping OldPageMapping);

#endif//NS64_MM_H