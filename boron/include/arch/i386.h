/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	arch/i386.h
	
Abstract:
	This header file defines architecture specifics for the
	i386 CPU architecture.
	
Author:
	iProgramInCpp - 14 August 2025
***/
#pragma once

#ifndef TARGET_I386
#error "Don't include this if you aren't building for i386!"
#endif



// ======== used by the Memory Manager ========

#ifdef KERNEL

// for recursive paging, PML2 index will be 1023 - 111111111b.
#define MI_RECURSIVE_PAGING_START (1023)
// Note: The PML2 itself will be accessible at 0xFFFFF000.
#define MI_PML2_LOCATION ((uintptr_t)0xFFFFF000)
#define MI_PML1_LOCATION ((uintptr_t)0xFFC00000)
#define MI_PML_ADDRMASK  ((uintptr_t)0xFFFFF000)

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

#endif

#define MM_KERNEL_SPACE_BASE (0x80000000)
#define MM_USER_SPACE_END    (0x7FFFFFFF)

// memory layout:
// ?

#define MM_PFNDB_BASE        (0xF0000000)

// Ppage table entry bits
#define MM_PTE_PRESENT    (1 << 0)
#define MM_PTE_READWRITE  (1 << 1)
#define MM_PTE_USERACCESS (1 << 2)
#define MM_PTE_WRITETHRU  (1 << 3)
#define MM_PTE_CDISABLE   (1 << 4)
#define MM_PTE_ACCESSED   (1 << 5)
#define MM_PTE_DIRTY      (1 << 6)
#define MM_PTE_PAT        (1 << 7)
#define MM_PTE_GLOBAL     (1 << 8)
#define MM_PTE_ISFROMPMM  (1 << 9)
#define MM_PTE_COW        (1 << 10) // TODO: We are supposed to be phasing this one out.
#define MM_PTE_TRANSITION (1 << 11)
#define MM_PTE_NOEXEC     (0 << 0)  // you cannot mark things as NX on x86 without PAE.

#define MM_PTE_ADDRESSMASK (0xFFFFF000)

// Disabled PTE
#define MM_DPTE_DECOMMITTED  (1 << 3)
#define MM_DPTE_COMMITTED    (1 << 8)
//#define MM_DPTE_BACKEDBYFILE
//#define MM_DPTE_SWAPPED
#define MM_DPTE_WASPRESENT   (1 << 11)

// Page fault reasons
#define MM_FAULT_PROTECTION (1ULL << 0) // 0: Page wasn't marked present; 1: Page protection violation (e.g. writing to a readonly page)
#define MM_FAULT_WRITE      (1ULL << 1) // 0: Caused by a read; 1: Caused by a write
#define MM_FAULT_USER       (1ULL << 2) // 1: Fault was caused in user mode
#define MM_FAULT_INSNFETCH  (1ULL << 4) // 1: Attempted to execute code from a page marked with the NOEXEC bit

// Page size
#define PAGE_SIZE (0x1000)

typedef uint32_t MMPTE, *PMMPTE;

#define PML1_IDX(addr) (((addr) >> 12) & 0x3FF)
#define PML2_IDX(addr) (((addr) >> 22) & 0x3FF)

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
	
	uint32_t ebp;
	uint32_t sfra; // stack frame return address
	
	// Registers pushed by KiTrapCommon.  Pushed in reverse order from how they're laid out.
	uint32_t cr2;
	uint32_t edi, esi;
	uint32_t cs_dupl;
	uint32_t edx, ecx, ebx, eax;
	
	uint32_t IntNumber;
	uint32_t ErrorCode;
	
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	
	// N.B.  These are invalid if CS is in kernel mode!
	uint32_t esp;
	uint32_t ss;
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
	uint32_t OffsetLow  : 16;
	// bytes 2 and 3
	uint32_t SegmentSel : 16;
	// byte 4
	uint32_t Reserved0  : 8;
	// byte 5
	uint32_t GateType   : 4;
	uint32_t Reserved1  : 1;
	uint32_t DPL        : 2;
	uint32_t Present    : 1;
	// bytes 6, 7
	uint32_t OffsetHigh : 16;
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
#define SEG_NULL        (0x00)
#define SEG_RING_0_CODE (0x08)
#define SEG_RING_0_DATA (0x10)
#define SEG_RING_3_CODE (0x18)
#define SEG_RING_3_DATA (0x20)
#define C_GDT_SEG_COUNT (5)

typedef struct
{
	uint16_t Link, Res0;
	uint32_t Esp0, Ss0;
	uint32_t Esp1, Ss1;
	uint32_t Esp2, Ss2;
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
	uint32_t Es;
	uint32_t Cs;
	uint32_t Ss;
	uint32_t Ds;
	uint32_t Fs;
	uint32_t Gs;
	uint32_t Ldtr;
	uint16_t Res1, Iopb;
	uint32_t Ssp;
}
PACKED
KTSS;

typedef struct KGDT_ENTRY_tag
{
	uint64_t Limit1 : 16;
	uint64_t Base1  : 24;
	uint64_t Access : 8;
	uint64_t Limit2 : 4;
	uint64_t Flags  : 4;
	uint64_t Base2  : 8;
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

#include <arch.h>
