// Boron - Memory Manager
#ifndef NS64_MM_H
#define NS64_MM_H

#include <main.h>

#ifdef TARGET_X86_64

// memory layout:
// hhdm base - FFFF'8000'0000'0000 - FFFF'80FF'FFFF'FFFF (usually. Could be different, but it'd better not be!)
// PFN DB    - FFFF'A000'0000'0000 - FFFF'A00F'FFFF'FFFF (for the physical addresses, doubt you need more than 48 bits)
#define MM_PFNDB_BASE (0xFFFFA00000000000)

// Page table entry bits
#define MM_PTE_PRESENT    (1ULL <<  0)
#define MM_PTE_READWRITE  (1ULL <<  1)
#define MM_PTE_SUPERVISOR (1ULL <<  2)
#define MM_PTE_WRITETHRU  (1ULL <<  3)
#define MM_PTE_CDISABLE   (1ULL <<  4)
#define MM_PTE_ACCESSED   (1ULL <<  5)
#define MM_PTE_DIRTY      (1ULL <<  6)
#define MM_PTE_PAT        (1ULL <<  7)
#define MM_PTE_GLOBAL     (1ULL <<  8) // doesn't invalidate the pages from the TLB when CR3 is changed
#define MM_PTE_AVAIL9     (1ULL <<  9) // these available bits will be used later
#define MM_PTE_AVAIL10    (1ULL << 10)
#define MM_PTE_AVAIL11    (1ULL << 11)
#define MM_PTE_NOEXEC     (1ULL << 63) // aka eXecute Disable

#define MM_PTE_ADDRESSMASK (0x000FFFFFFFFFF000) // description of the other bits that aren't 1 in the mask:
	// 63 - execute disable
	// 62..59 - protection key (unused)
	// 58..52 - more available bits

#define PAGE_SIZE (0x1000)

// bits 0.11   - Offset within the page
// bits 12..20 - Index within the PML1
// bits 21..29 - Index within the PML2
// bits 30..38 - Index within the PML3
// bits 39..47 - Index within the PML4
// bits 48..63 - Sign extension of the 47th bit
#define PML1_IDX(addr)  (((idx) >> 12) & 0x1FF)
#define PML2_IDX(addr)  (((idx) >> 21) & 0x1FF)
#define PML3_IDX(addr)  (((idx) >> 30) & 0x1FF)
#define PML4_IDX(addr)  (((idx) >> 39) & 0x1FF)

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