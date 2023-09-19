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

// ====== Platform independent interface for page table management ======
// WARNING! This is not thread safe! So please take care of thread safety yourself.

// Creates a page mapping.
PageMapping MmCreatePageMapping(PageMapping OldPageMapping);

// Deletes a page mapping.
void MmFreePageMapping(PageMapping OldPageMapping);

// Resolves a page table entry pointer (virtual address offset by HHDM) relative to an address.
// Can allocate the missing page mapping levels on its way if the flag is set.
// If on its way, it hits a higher page size, currently it will return null since it's not really
// designed for that..
PageTableEntry* MmGetPTEPointer(PageMapping Mapping, uintptr_t Address, bool AllocateMissingPMLs);

// Attempts to map a physical page into the specified address space. Placeholder Function
bool MmMapAnonPage(PageMapping Mapping, uintptr_t Address, uintptr_t Permissions);

// Unmaps some memory. Automatically frees it if it is handled by the PMM.
void MmUnmapPages(PageMapping Mapping, uintptr_t Address, size_t LengthPages); 

// Handles a page fault. Returns whether or not the page fault was handled.
bool MmPageFault(uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode);

// Issue a TLB shootdown request. This is the official API for this purpose.
void MmIssueTLBShootDown(uintptr_t Address, size_t LengthPages);

#endif//NS64_MM_H