// Boron64 - AMD64 specific platform definitions
#ifndef NS64_HAL_AMD64_H
#define NS64_HAL_AMD64_H

#ifndef TARGET_AMD64
#error "Don't include this if you aren't building for amd64"
#endif

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
	uint64_t rdx, rcx, rbx;
	
	// The interrupt type we are handling. Neither the values nor its presence are
	// platform specific. This field is guaranteed to appear in all HALs.
	uint64_t int_type;
	
	// This is done for speed.
	uint64_t rax;
	
	// Registers pushed by the CPU when handling the interrupt.
	uint64_t error_code; // only sometimes pushed - we'll push a 0 in the cases where that's not done
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

#endif//NS64_HAL_AMD64_H
