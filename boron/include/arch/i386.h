/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	arch/i386.h
	
Abstract:
	This header file contains the constant definitions for
	the i386 platform.
	
Author:
	iProgramInCpp - 14 October 2025
***/
#pragma once

#ifdef KERNEL

// start PML2 index will be 512.  The PFN database's is 776
#define MI_GLOBAL_AREA_START     (512)
#define MI_GLOBAL_AREA_START_2ND (832)

#define MI_RECURSIVE_PAGING_START (511)

#define MI_PML2_LOCATION ((uintptr_t)0xFFFFF000U)
#define MI_PML1_LOCATION ((uintptr_t)0xFFC00000U)
#define MI_PML1_LOC_END  ((uint64_t)0x100000000U)
#define MI_PML_ADDRMASK  ((uintptr_t)0xFFFFF000U)

// MmGetHHDMOffsetAddr and other HHDM-related calls aren't implemented
// using an HHDM on 32-bit.  Instead, they're implemented via 8MB windows
// that get spawned in everytime MmGetHHDMOffsetAddr is called with a
// different 8MB region.
#define MI_FASTMAP_START ((uintptr_t)0xC0800000U)
#define MI_FASTMAP_MASK  ((uintptr_t)0xFF800000U)

typedef union
{
	struct
	{
		uintptr_t PageOffset : 12;
		uintptr_t Level1Index : 10;
		uintptr_t Level2Index : 10;
	};
	
	uintptr_t Long;
}
MMADDRESS_CONVERT;

#define MM_KERNEL_SPACE_BASE (0x80000000U)
#define MM_USER_SPACE_END    (0x7FFFFFFFU)

#define MM_PFNDB_BASE     (0xC2000000U)

#define MM_PTE_PRESENT    (1 <<  0)
#define MM_PTE_READWRITE  (1 <<  1)
#define MM_PTE_USERACCESS (1 <<  2)
#define MM_PTE_WRITETHRU  (1 <<  3)
#define MM_PTE_CDISABLE   (1 <<  4)
#define MM_PTE_ACCESSED   (1 <<  5)
#define MM_PTE_DIRTY      (1 <<  6)
#define MM_PTE_PAT        (1 <<  7)
#define MM_PTE_PAGESIZE   (1 <<  7) // for 4MB pages. not used by the kernel
#define MM_PTE_GLOBAL     (1 <<  8) // doesn't invalidate the pages from the TLB when CR3 is changed
#define MM_PTE_ISFROMPMM  (1 <<  9) // if the allocated memory is managed by the PFN database
#define MM_PTE_COW        (1 << 10) // if this page is to be copied after a write

#define MM_PTE_NOEXEC     (0)       // no such thing on 32-bit (without PAE)
#define MM_PTE_PKMASK     (0)       // no such thing on 32-bit

#define MM_PTE_ADDRESSMASK (0xFFFFF000U)

// Disabled PTE (present bit is zero):
// bits 0..2 - Permission bits as usual
// bit  3   - Is decommitted (was previously committed but is no longer)
// bit  8   - Is demand paged
// bit  9   - 0 if from PMM (anonymous), 1 if mapped from a file
// bit  10  - Was swapped to pagefile (2)
// bit  30  - used by the unmap code, see (1)

// NOTES:
//
// (1) - If MM_DPTE_WASPRESENT is set, it's treated as a regular PTE in terms of flags, except that MM_PTE_PRESENT is zero.
//       It contains a valid PMM address which should be freed.
//
// (2) - If MM_DPTE_SWAPPED is set, bits 52...12 represent the offset into the pagefile, and bits 57...53 mean the pagefile index.
//
// (3) - If the PTE is in transition, then the physical page is part of either the standby or the modified page list.

#define MM_DPTE_DECOMMITTED  (1 << 3)
#define MM_DPTE_COMMITTED    (1 << 8)
#define MM_DPTE_BACKEDBYFILE (1 << 9)
#define MM_DPTE_SWAPPED      (1 << 10)
#define MM_DPTE_WASPRESENT   (1 << 30)
#define MM_PTE_ISPOOLHDR     (1 << 31) // if the PTE actually contains the address of a pool entry (subtracted MM_KERNEL_SPACE_BASE from it)

// Page fault reasons
#define MM_FAULT_PROTECTION (1 << 0) // 0: Page wasn't marked present; 1: Page protection violation (e.g. writing to a readonly page)
#define MM_FAULT_WRITE      (1 << 1) // 0: Caused by a read; 1: Caused by a write
#define MM_FAULT_USER       (1 << 2) // 1: Fault was caused in user mode
#define MM_FAULT_INSNFETCH  (1 << 0) // 1: Attempted to execute code from a page marked with the NOEXEC bit

#define PAGE_SIZE (0x1000)

typedef uint32_t MMPTE, *PMMPTE;

// bits 0.11   - Offset within the page
// bits 12..21 - Index within the PML1
// bits 22..31 - Index within the PML2
#define PML1_IDX(addr)  (((addr) >> 12) & 0x3FF)
#define PML2_IDX(addr)  (((addr) >> 22) & 0x3FF)

// ======== model specific registers ========
#define MSR_FS_BASE        (0xC0000100)
#define MSR_GS_BASE        (0xC0000101)
#define MSR_GS_BASE_KERNEL (0xC0000102)
#define MSR_IA32_EFER      (0xC0000080)
#define MSR_IA32_STAR      (0xC0000081)
#define MSR_IA32_LSTAR     (0xC0000082)
#define MSR_IA32_FMASK     (0xC0000084)

#define MSR_IA32_EFER_SCE (1 << 0)  // SYSCALL/SYSRET enable
#define MSR_IA32_EFER_LME (1 << 8)  // Long Mode enable - Limine sets this
#define MSR_IA32_EFER_LMA (1 << 10) // Long Mode active
#define MSR_IA32_EFER_NXE (1 << 11) // No Execute enable

struct KREGISTERS_tag
{
	// Old IPL
	uint32_t OldIpl;
	
	uint32_t Ebp;
	uint32_t Sfra; // stack frame return address
	
	// Registers pushed by KiTrapCommon. Pushed in reverse order from how they're laid out.
	uint32_t Cr2;
	uint32_t Edi, Esi;
	uint32_t CsDupl;
	uint32_t Edx, Ecx, Ebx, Eax;
	
	// Registers pushed by each trap handler
	uint32_t IntNumber;
	uint32_t ErrorCode;
	
	// Registers pushed by the CPU when handling the interrupt
	uint32_t Eip;
	uint32_t Cs;
	uint32_t Eflags;
	// NOTE: these are only valid and used if the privilege level is different!
	// (e.g. if we interrupted user mode), so DO NOT rely on setting these
	// when returning to kernel mode
	uint32_t Esp;
	uint32_t Ss;
};

#include <arch/ipl.h>

// IDT
#define C_IDT_MAX_ENTRIES (0x100)

#define INTV_DBL_FAULT  (0x08)
#define INTV_PROT_FAULT (0x0D)
#define INTV_PAGE_FAULT (0x0E)

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
	// bytes 6, 7
	uint64_t OffsetHigh : 16;
}
PACKED
KIDT_ENTRY, *PKIDT_ENTRY;

typedef struct
{
	KIDT_ENTRY Entries[C_IDT_MAX_ENTRIES];
}
KIDT, *PKIDT;

// GDT

// Note: Data and code segments are *swapped* for user mode because
// for whatever reason AMD or intel decided they just *had* to take
// the value of IA32_STAR 63:48 and add 16 to it. for "no reason".
//
// surely there was a reason... right?
#define SEG_NULL        (0x00)
#define SEG_RING_0_CODE (0x08)
#define SEG_RING_0_DATA (0x10)
#define SEG_RING_3_DATA (0x18)
#define SEG_RING_3_CODE (0x20)
#define C_GDT_SEG_COUNT (5)

// note: not packed, so this struct will get padding after each uint16_t
typedef struct
{
	uint16_t Link;
	uint32_t Esp0;
	uint16_t Ss0;
	uint32_t Esp1;
	uint16_t Ss1;
	uint32_t Esp2;
	uint16_t Ss2;
	uint32_t Cr3;
	uint32_t Eip;
	uint32_t Eflags;
	uint32_t Eax;
	uint32_t Ecx;
	uint32_t Edx;
	uint32_t Ebx;
	uint32_t Esp;
	uint32_t Ebp;
	uint32_t Esi;
	uint32_t Edi;
	uint16_t Es, Pad0;
	uint16_t Cs, Pad1;
	uint16_t Ss, Pad2;
	uint16_t Ds, Pad3;
	uint16_t Fs, Pad4;
	uint16_t Gs, Pad5;
	uint16_t Ldtr, Pad6;
	uint16_t Pad7, Iopb;
	uint32_t Ssp;
}
KTSS;

typedef union KGDT_ENTRY_tag
{
	struct
	{
		uint64_t Limit1 : 16;
		uint64_t Base1  : 24;
		uint64_t Access : 8;
		uint64_t Limit2 : 4;
		uint64_t Flags  : 4;
		uint64_t Base2  : 16;
	}
	PACKED;
	
	uint64_t Entry;
}
PACKED
KGDT_ENTRY;

// Global Descriptor Table
typedef struct
{
	uint64_t   Segments[C_GDT_SEG_COUNT];
	KGDT_ENTRY TssEntry;
}
KGDT;

typedef struct
{
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

#endif
