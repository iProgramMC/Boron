#ifndef NS64_ARCH_AMD64_H
#define NS64_ARCH_AMD64_H

#ifndef TARGET_AMD64
#error "Don't include this if you aren't building for amd64!"
#endif

// Model specific registers
uint64_t KeGetMSR(uint32_t msr);
void KeSetMSR(uint32_t msr, uint64_t value);

// Port I/O
uint8_t KePortReadByte(uint16_t portNo);
void KePortWriteByte(uint16_t portNo, uint8_t data);
uint16_t KePortReadWord(uint16_t portNo);
void KePortWriteWord(uint16_t portNo, uint16_t data);
uint32_t KePortReadDword(uint16_t portNo);
void KePortWriteDword(uint16_t portNo, uint32_t data);

// ======== used by the Memory Manager ========

#ifdef KERNEL

// start PML4 index will be 321. The PFN database's is 320
#define MI_GLOBAL_AREA_START (321ULL)

// for recursive paging, PML4 index will be 322 - 101000010b.
#define MI_RECURSIVE_PAGING_START (322ULL)
// Note: The PML4 itself will be accessible at the address:
// 1111111111111111 101000010 101000010 101000010 101000010 000000000000
// Which translates to 0xFFFFA150A8542000.

#define MI_PML3_LOCATION ((uintptr_t)0xFFFFA150A8542000ULL)
#define MI_PML2_LOCATION ((uintptr_t)0xFFFFA150A8400000ULL)
#define MI_PML1_LOCATION ((uintptr_t)0xFFFFA10000000000ULL)
#define MI_PML1_LOC_END  ((uintptr_t)0xFFFFA18000000000ULL)
#define MI_PML_ADDRMASK  ((uintptr_t)0x0000FFFFFFFFF000ULL)

#define MI_PTE_LOC(Address) (MI_PML1_LOCATION + ((Address & MI_PML_ADDRMASK) >> 12) * sizeof(MMPTE))

typedef union
{
	struct
	{
		uintptr_t PageOffset : 12;
		uintptr_t Level1Index : 9;
		uintptr_t Level2Index : 9;
		uintptr_t Level3Index : 9;
		uintptr_t Level4Index : 9;
		uintptr_t SignExtension : 16;
	};
	
	uintptr_t Long;
}
MMADDRESS_CONVERT;

#endif

// Note! Most of these are going to be present everywhere we'll port to.

#define MM_KERNEL_SPACE_BASE (0xFFFF800000000000)
#define MM_USER_SPACE_END    (0x00007FFFFFFFFFFF)

// memory layout:
// hhdm base - FFFF'8000'0000'0000 - FFFF'80FF'FFFF'FFFF (usually. Could be different, but it'd better not be!)
// PFN DB    - FFFF'A000'0000'0000 - FFFF'A00F'FFFF'FFFF (for the physical addresses, doubt you need more than 48 bits)
#define MM_PFNDB_BASE (0xFFFFA00000000000) // PML4 index: 320

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
#define MM_PTE_COW        (1ULL << 10) // if this page is to be copied after a write
#define MM_PTE_TRANSITION (1ULL << 11) // if this page is in transition (3)
#define MM_PTE_NOEXEC     (1ULL << 63) // aka eXecute Disable
#define MM_PTE_PKMASK     (15ULL<< 59) // protection key mask. We will not use it.
#define MM_PTE_ISPOOLHDR  (1ULL << 58) // if the PTE actually contains the address of a pool entry (subtracted MM_KERNEL_SPACE_BASE from it)

#define MM_PTE_ADDRESSMASK (0x000FFFFFFFFFF000) // description of the other bits that aren't 1 in the mask:
	// 63 - execute disable
	// 62..59 - protection key (unused)
	// 58..52 - more available bits

// Disabled PTE (present bit is zero):
// bits 0..2 and 63 - Permission bits as usual
// bit  3           - Is decommitted (was previously committed but is no longer)
// bit  8           - Is demand paged
// bit  9           - 0 if from PMM (anonymous), 1 if mapped from a file
// bit  10          - Was swapped to pagefile (2)
// bit  62          - used by the unmap code, see (1)

// NOTES:
//
// (1) - If MM_DPTE_WASPRESENT is set, it's treated as a regular PTE in terms of flags, except that MM_PTE_PRESENT is zero.
//       It contains a valid PMM address which should be freed.
//
// (2) - If MM_DPTE_SWAPPED is set, bits 52...12 represent the offset into the pagefile, and bits 57...53 mean the pagefile index.
//
// (3) - If the PTE is in transition, then the physical page is part of either the standby or the modified page list.

#define MM_DPTE_DECOMMITTED  (1ULL << 3)
#define MM_DPTE_COMMITTED    (1ULL << 8)
#define MM_DPTE_BACKEDBYFILE (1ULL << 9)
#define MM_DPTE_SWAPPED      (1ULL << 10)
#define MM_DPTE_WASPRESENT   (1ULL << 62)

// Page fault reasons
#define MM_FAULT_PROTECTION (1ULL << 0) // 0: Page wasn't marked present; 1: Page protection violation (e.g. writing to a readonly page)
#define MM_FAULT_WRITE      (1ULL << 1) // 0: Caused by a read; 1: Caused by a write
#define MM_FAULT_USER       (1ULL << 2) // 1: Fault was caused in user mode
#define MM_FAULT_INSNFETCH  (1ULL << 4) // 1: Attempted to execute code from a page marked with the NOEXEC bit

#define PAGE_SIZE (0x1000)

typedef uint64_t MMPTE, *PMMPTE;

// bits 0.11   - Offset within the page
// bits 12..20 - Index within the PML1
// bits 21..29 - Index within the PML2
// bits 30..38 - Index within the PML3
// bits 39..47 - Index within the PML4
// bits 48..63 - Sign extension of the 47th bit
#define PML1_IDX(addr)  (((addr) >> 12) & 0x1FF)
#define PML2_IDX(addr)  (((addr) >> 21) & 0x1FF)
#define PML3_IDX(addr)  (((addr) >> 30) & 0x1FF)
#define PML4_IDX(addr)  (((addr) >> 39) & 0x1FF)

// ======== model specific registers ========
#define MSR_FS_BASE        (0xC0000100)
#define MSR_GS_BASE        (0xC0000101)
#define MSR_GS_BASE_KERNEL (0xC0000102)

struct KREGISTERS_tag
{
	// Old IPL
	uint64_t OldIpl;
	
	uint64_t rbp;
	uint64_t sfra; // stack frame return address
	
	// Registers pushed by KiTrapCommon. Pushed in reverse order from how they're laid out.
	uint64_t cr2;
	uint64_t rdi, rsi;
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rdx;
	
	uint64_t rcx, rbx, rax;
	
	// Registers pushed by each trap handler
	uint64_t IntNumber; // the interrupt vector
	uint64_t ErrorCode; // only sometimes actually pushed by the CPU, if not, it's a zero
	
	// Registers pushed by the CPU when handling the interrupt.
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

#include <arch/ipl.h>

// IDT
#define C_IDT_MAX_ENTRIES (0x100)

#define INTV_DBL_FAULT  (0x08)
#define INTV_PROT_FAULT (0x0D)
#define INTV_PAGE_FAULT (0x0E)
//#define INTV_DPC_IPI    (0x40)
//#define INTV_APIC_TIMER (0xF0)
//#define INTV_TLBS_IPI   (0xFD)
//#define INTV_CRASH_IPI  (0xFE)
//#define INTV_SPURIOUS   (0xFF)

typedef struct KIDT_ENTRY_tag
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
KIDT_ENTRY, *PKIDT_ENTRY;

typedef struct
{
	KIDT_ENTRY Entries[C_IDT_MAX_ENTRIES];
}
KIDT, *PKIDT;

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
KTSS;

// This is pretty much the same as a GDT entry except that there's an extra 8 bytes.
typedef struct KTSS_ENTRY_tag
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
KTSS_ENTRY;

// Global Descriptor Table
typedef struct
{
	uint64_t   Segments[C_GDT_SEG_COUNT];
	KTSS_ENTRY TssEntry;
}
KGDT;

typedef struct
{
	void* IntStack;
	KGDT Gdt;
	KTSS Tss;
}
KARCH_DATA, *PKARCH_DATA;

// Manual interrupt disabling functions. Should instead use
// KeDisableInterrupts and KeRestoreInterrupts, but these have
// their uses too.
#define DISABLE_INTERRUPTS() ASM("cli")
#define ENABLE_INTERRUPTS()  ASM("sti")

// MSI message data register
#define MSI_TRIGGERLEVEL   (1 << 15)
#define MSI_LEVELASSERT    (1 << 14)

#include <arch.h>

#endif//NS64_ARCH_AMD64_H
