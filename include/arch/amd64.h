#ifndef NS64_ARCH_AMD64_H
#define NS64_ARCH_AMD64_H

#ifndef TARGET_AMD64
#error "Don't include this if you aren't building for amd64!"
#endif

// Model specific registers
uint64_t KeGetMSR(uint32_t msr);
void KeSetMSR(uint32_t msr, uint64_t value);

// ======== used by the Memory Manager ========

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
#define MM_PTE_PAGESIZE   (1ULL <<  7) // in terms of PML3/PML2 entries, for 1GB/2MB pages respectively. Not Used by the kernel
#define MM_PTE_GLOBAL     (1ULL <<  8) // doesn't invalidate the pages from the TLB when CR3 is changed
#define MM_PTE_ISFROMPMM  (1ULL <<  9) // if the allocated memory is managed by the PFN database
#define MM_PTE_WASRDWR    (1ULL << 10) // if after cloning, the page was read/write
#define MM_PTE_DEMAND     (1ULL << 11) // is waiting for an allocation (demand paging)
#define MM_PTE_NOEXEC     (1ULL << 63) // aka eXecute Disable

#define MM_PTE_ADDRESSMASK (0x000FFFFFFFFFF000) // description of the other bits that aren't 1 in the mask:
	// 63 - execute disable
	// 62..59 - protection key (unused)
	// 58..52 - more available bits

#define PAGE_SIZE (0x1000)

typedef uint64_t PageTableEntry;

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

// ======== model specific registers ========
#define MSR_FS_BASE        (0xC0000100)
#define MSR_GS_BASE        (0xC0000101)
#define MSR_GS_BASE_KERNEL (0xC0000102)

struct CPUState
{
	// Registers pushed by our entry code. Pushed in reverse order from how they're laid out.
	uint16_t ds, es, fs, gs;
	uint64_t cr2;
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi;
	uint64_t rdx, rcx, rbx, rax;
	
	// Registers pushed by the CPU when handling the interrupt.
	uint64_t error_code; // only sometimes pushed - we'll push a 0 in the cases where that's not done
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

typedef enum eIPL
{
	IPL_NORMAL = 0x0, // business as usual
	IPL_APC    = 0x3, // asynch procedure calls. Page faults only allowed up to this IPL
	IPL_DPC    = 0x4, // deferred procedure calls and the scheduler
	IPI_CLOCK  = 0xE, // for clock timers
	IPL_NOINTS = 0xF, // total control of the CPU. Interrupts are disabled in this IPL and this IPL only.
}
eIPL;

// IDT
#define C_IDT_MAX_ENTRIES (0x100)

#define INTV_DBL_FAULT  (0x08) // just an outright crash
#define INTV_PAGE_FAULT (0x0E) // exempt from the IPL stuff, but any page fault above IPL_APC is considered an error
#define INTV_DPC_IPI    (0x50)
#define INTV_APIC_TIMER (0xF0)
#define INTV_CRASH_IPI  (0xFE)
#define INTV_SPURIOUS   (0xFF)

typedef struct
{
	// bytes 0 and 1
	uint64_t OffsetLow  : 16;
	// bytes 2 and 3
	uint64_t SegmentSel : 16;
	// byte 4
	uint64_t IST        : 3;
	uint64_t Reserved0  : 5;
	// byte 5
	uint64_t GateType   : 4;
	uint64_t Reserved1  : 1;
	uint64_t DPL        : 2;
	uint64_t Present    : 1;
	// bytes 6, 7, 8, 9, 10, 11
	uint64_t OffsetHigh : 48;
	// bytes 12, 13, 14, 15
	uint64_t Reserved2  : 32;
}
PACKED
IDTEntry;

typedef struct
{
	IDTEntry Entries[C_IDT_MAX_ENTRIES];
}
IDT;

// GDT

#define SEG_NULL        (0x00)
#define SEG_RING_0_CODE (0x08)
#define SEG_RING_0_DATA (0x10)
#define SEG_RING_3_CODE (0x18)
#define SEG_RING_3_DATA (0x20)
#define C_GDT_SEG_COUNT (5)

typedef struct
{
	uint32_t Reserved0;
	uint64_t RSP[3]; // RSP 0-2
	uint64_t Reserved1;
	uint64_t IST[7]; // IST 1-7
	uint64_t Reserved2;
	uint16_t Reserved3;
	uint16_t IOPB;
}
PACKED
TSS;

// This is pretty much the same as a GDT entry except that there's an extra 8 bytes.
typedef struct
{
	uint64_t Limit1 : 16;
	uint64_t Base1  : 24;
	uint64_t Access : 8;
	uint64_t Limit2 : 4;
	uint64_t Flags  : 4;
	uint64_t Base2  : 40;
	uint64_t Resvd  : 32;
}
PACKED
TSSEntry;

// Global Descriptor Table
typedef struct
{
	uint64_t Segments[C_GDT_SEG_COUNT];
	TSSEntry TssEntry;
}
GDT;

typedef struct
{
	void* IntStack;
	GDT Gdt;
	TSS Tss;
}
KeArchData;

#include <arch.h>

#endif//NS64_ARCH_AMD64_H
