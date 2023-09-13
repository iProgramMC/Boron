// Boron - Memory Manager
#ifndef NS64_MM_H
#define NS64_MM_H

#include <main.h>
#include <arch.h>

// handle to a page mapping.
typedef uintptr_t PageMapping;

// Page frame item in the page frame database.
// Keep this a power of 2, please.
// Currently this only reserves 0.7% of physical memory.  Not bad
typedef struct
{
	// Flags for the specifically referenced page frame.
	int Type : 4;
	int Flags : 28;
	
	// Disregard if this is allocated.  Eventually these will be part of a union
	// where they will take on different roles depending on the role of the page.
	int NextFrame;
	int PrevFrame;
	
	// Disregard if this is free. Eventually this will be part of a union
	// where it will take on different roles depending on the role of the
	// page.
	int RefCount;
	
	// The PTE this is used in. Used only in non-shared pages.
	uint64_t PteUsingThis;
	
	// Unused for now.
	uint64_t qword3;
}
PageFrame;

enum ePageFrameType
{
	PF_TYPE_FREE,
	PF_TYPE_ZEROED,
	PF_TYPE_USED,
};

#define PFN_INVALID (-1)

#define PFNS_PER_PAGE (PAGE_SIZE / sizeof(PageFrame))

_Static_assert((sizeof(PageFrame) & (sizeof(PageFrame) - 1)) == 0,  "THe page frame struct should be a power of two");

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