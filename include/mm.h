// Boron - Memory Manager
#ifndef NS64_MM_H
#define NS64_MM_H

#include <main.h>
#include <hal.h>

#ifdef TARGET_AMD64

#endif

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
void MiInitPMM();
uint8_t* MmGetHHDMBase();
void* MmGetHHDMOffsetAddr(uintptr_t physAddr);
int MmPhysPageToPFN(uintptr_t physAddr);
uintptr_t MmPFNToPhysPage(int pfn);

// this returns a PFN number, use MmPFNToPhysPage to convert it to
// a physical address, and MmGetHHDMOffsetAddr to address into it directly
int MmAllocatePhysicalPage();

// this expects a PFN number. Use MmPhysPageToPFN if you have a physically
// addressed page to free.
void MmFreePhysicalPage(int pfn);

void MmPmmDumpState();

#endif//NS64_MM_H