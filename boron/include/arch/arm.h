#pragma once

#ifndef TARGET_ARM
#error "Don't include this if you aren't building for armv6/armv7!"
#endif

#include <arch/ipl.h>

#ifdef KERNEL

// start PML2 index will be 512.  The PFN database's is 776
#define MI_GLOBAL_AREA_START     (512)
#define MI_GLOBAL_AREA_START_2ND (896)

#define MI_RECURSIVE_PAGING_START (1023)

#define MI_PML2_LOCATION ((uintptr_t)0xFF800000U) // Debbie (L2 page tables)
#define MI_PML1_LOCATION ((uintptr_t)0xFFC00000U) // Jibbie (L1 page table + Jibbie + Debbie)

#define MI_PML1_MIRROR_LOCATION ((uintptr_t)(0xFFC04000U))
#define MI_PML2_MIRROR_LOCATION ((uintptr_t)(0xFFC05000U))

#define MI_PML_ADDRMASK  ((uintptr_t)0xFFFFF000U)

#endif // KERNEL

// MmGetHHDMOffsetAddr and other HHDM-related calls are implemented two-fold:
//
// - The first 256 MB of RAM are mapped in an offset identity mapping
//
// - The rest of the address space is accessible via a 16 MB window fast mapping.
#define MI_IDENTMAP_START ((uintptr_t) 0xC0000000)
#define MI_IDENTMAP_SIZE  ((uintptr_t) 0x10000000)

#define MI_IDENTMAP_START_PHYS ((uintptr_t) 0x00000000)

#define MI_FASTMAP_START ((uintptr_t) 0xD0000000)
#define MI_FASTMAP_MASK  ((uintptr_t) 0xFFFF0000)
#define MI_FASTMAP_SIZE  (64 * 1024)

typedef union
{
	// the ARMARM mentions L1 as being the top level.
	// i386.h has them swapped, so keep that in mind!
	struct
	{
		uintptr_t PageOffset : 12;
		uintptr_t Level2Index : 8;
		uintptr_t Level1Index : 12;
	};
	
	uintptr_t Long;
}
MMADDRESS_CONVERT;

#define MM_KERNEL_SPACE_BASE (0x80000000U)
#define MM_USER_SPACE_END    (0x7FFFFFFFU)

#define MM_PFNDB_BASE     (0xD4000000U)

// -- L1 PTEs --
#define MM_ARM_PTEL1_TYPE              (3U << 0)
#define MM_ARM_PTEL1_COARSE_PAGE_TABLE (1U << 0) // Type = b01, Coarse Page Table
#define MM_ARM_PTEL1_SECTION_SETUP     ((3U << 2) | (7U << 12) | (1U << 10) | (2U << 0)) // CB = 0b11, TEX = 0b111, AP = 0b01, APX=0, Type = 0b10

// -- L2 PTEs --

#ifdef TARGET_ARMV5

// ARMv5 first level PTEs aren't different, but second level PTEs are.
#define MM_ARM_PTEL2_B (1 << 2)
#define MM_ARM_PTEL2_C (1 << 3)

#define MM_ARM_PTEL2_AP_NOACCESS       ((0U << 4)) // super N/A, user N/A
#define MM_ARM_PTEL2_AP_SUPERREADWRITE ((1U << 4)) // super R/W, user N/A
#define MM_ARM_PTEL2_AP_USERREADONLY   ((2U << 4)) // super R/W, user R/O
#define MM_ARM_PTEL2_AP_USERREADWRITE  ((3U << 4)) // super R/W, user R/W

#define MM_ARM_PTEL2_AP_ALL(x) ((x) | ((x) << 2) | ((x) << 4) | ((x) << 6))
#define MM_ARM_PTEL2_AP_ALL_NOACCESS       (0)
#define MM_ARM_PTEL2_AP_ALL_SUPERREADWRITE MM_ARM_PTEL2_AP_ALL(MM_ARM_PTEL2_AP_SUPERREADWRITE)
#define MM_ARM_PTEL2_AP_ALL_USERREADONLY   MM_ARM_PTEL2_AP_ALL(MM_ARM_PTEL2_AP_USERREADONLY)
#define MM_ARM_PTEL2_AP_ALL_USERREADWRITE  MM_ARM_PTEL2_AP_ALL(MM_ARM_PTEL2_AP_USERREADWRITE)

#define MM_ARM_AP_MASK MM_ARM_PTEL2_AP_ALL(3U)

#define MM_ARM_PTEL2_TYPE_TRANSFAULT  (0U << 0)
#define MM_ARM_PTEL2_TYPE_LARGEPAGE   (1U << 0)
#define MM_ARM_PTEL2_TYPE_SMALLPAGE   (2U << 0)
#define MM_ARM_PTEL2_TYPE_EXSMALLPAGE (3U << 0)

#else // ARMv6

// AP = PTE[5:4], APX = PTE[9]
#define MM_ARM_PTEL2_AP_NOACCESS       ((0U << 4)) // super N/A, user N/A
#define MM_ARM_PTEL2_AP_SUPERREADWRITE ((1U << 4)) // super R/W, user N/A
#define MM_ARM_PTEL2_AP_USERREADONLY   ((2U << 4)) // super R/W, user R/O
#define MM_ARM_PTEL2_AP_USERREADWRITE  ((3U << 4)) // super R/W, user R/W
#define MM_ARM_PTEL2_AP_SUPERREADONLY  ((1U << 4) | (1U << 9)) // super R/O, user N/A
#define MM_ARM_PTEL2_AP_BOTHREADONLY   ((3U << 4) | (1U << 9)) // super R/O, user R/O

#define MM_ARM_AP_MASK ((1U << 9) | (3U << 4))

// TEX = PTE[8:6], C = PTE[3], B = PTE[2]
#define MM_ARM_PTEL2_TEXCB_STRONGORDER  ((0U << 6) | (0U << 2))
#define MM_ARM_PTEL2_TEXCB_SHAREDDEVICE ((0U << 6) | (1U << 2))
#define MM_ARM_PTEL2_TEXCB_CACHEABLE    ((7U << 6) | (3U << 2)) // b11 inner and outer policy.
#define MM_ARM_PTEL2_TEXCB_NORMALMEM    ((1U << 6) | (3U << 2)) // TEX=0b001, CB=0b11

#define MM_ARM_PTEL2_TEX_MASK (7U << 6)
#define MM_ARM_PTEL2_CB_MASK  (3U << 2)

// Type
#define MM_ARM_PTEL2_TYPE_TRANSFAULT  (0U << 0)
#define MM_ARM_PTEL2_TYPE_LARGEPAGE   (1U << 0)
#define MM_ARM_PTEL2_TYPE_SMALLPAGE   (2U << 0)
#define MM_ARM_PTEL2_TYPE_SMALLPAGENX (3U << 0)

#endif

// Disabled PTEs
// bits 9 and 5:4 - Permission bits as usual
// bit  2 - Is pool header
// bit  3 - Is committed
// bit  6 - Was present
// bit  7 - Is decommitted (was previously committed but is no longer)

#define MM_ARM_DPTE_ISPOOLHDR    (1U << 2)
#define MM_ARM_DPTE_COMMITTED    (1U << 3)
#define MM_ARM_DPTE_WASPRESENT   (1U << 6)
#define MM_ARM_DPTE_DECOMMITTED  (1U << 7)

// TODO: just assumes all of them are from PMM
#define MM_ARM_PTE_CHECKFROMPMM(Pte) (true)

#define MM_ARM_PTE_PFN(Pte) (((Pte) >> 12) & 0xFFFFF)
#define MM_ARM_PTE_NEWPFN(Pfn) ((Pfn) << 12)
#define MM_ARM_PTE_ADDRESSMASK (0xFFFFF000U)

// for the DFSR/IFSR, we currently only care about whether the page fault
// was due to a write or not.
#define MM_FAULT_WRITE (1U << 11) // RW bit

// strange sizes in here!
#define MM_L1PT_SIZE (0x4000) // 16K
#define MM_L2PT_SIZE (0x0400) // 1K

#define PAGE_SIZE (0x1000) // 4K

typedef uint32_t MMPTE_HW, *PMMPTE_HW;

#define PT_L1_IDX(addr) (((addr) >> 20) & 0xFFF)
#define PT_L2_IDX(addr) (((addr) >> 12) & 0xFF)

/*
// This struct represents all of the available registers at a time.
//
// User mode (EL0) has access to these registers. Interrupt handlers and
// the system may use different registers entirely. For example, FIQ mode
// replaces R8-R14 with its own set of registers using the ARM banking
// mechanism, namely R8_fiq - R14_fiq. This way, most FIQ handlers needn't
// even save any registers.
// The supervisor, abort, IRQ, and undefined modes each have a private copy
// of R13 (SP) and R14 (LR) as well (e.g. supervisor mode uses R13_svc and
// R14_svc)
struct KREGISTERS_tag
{
	uint32_t R0, R1, R2, R3;
	uint32_t R4, R5, R6, R7;
	uint32_t R8, R9, R10, R11;
	uint32_t R12, R13;
	
	union {
		struct {
			uint32_t R14, R15;
		};
		struct {
			uint32_t Lr, Pc;
		};
	};
	
	uint32_t Cpsr;
	uint32_t IntNumber;
};
*/

// This restrained struct represents only the registers saved in each IRQ handler.
struct KREGISTERS_tag
{
	uint32_t Cpsr;
	uint32_t R0;
	uint32_t R1;
	uint32_t R2;
	uint32_t R3;
	uint32_t R4;
	uint32_t R5;
	uint32_t R6;
	uint32_t R7;
	uint32_t Lr;
};

typedef struct
{
	int Dummy;
}
KTHREAD_ARCH_CONTEXT;

typedef struct
{
	int Dummy;
}
KARCH_DATA, *PKARCH_DATA;

#ifdef TARGET_ARMV5

#define DISABLE_INTERRUPTS() ASM(\
	"mrs r12, cpsr\n" \
	"orr r12, r12, #0x80\n" \
	"msr cpsr_c, r12\n" ::: "r12", "memory" \
)
#define ENABLE_INTERRUPTS() ASM(\
	"mrs r12, cpsr\n" \
	"bic r12, r12, #0x80\n" \
	"msr cpsr_c, r12\n" ::: "r12", "memory" \
)

#else

#define DISABLE_INTERRUPTS() ASM("cpsid i" ::: "memory");
#define ENABLE_INTERRUPTS()  ASM("cpsie i" ::: "memory");

#endif

FORCE_INLINE
void KeWaitForNextInterrupt()
{
#ifdef TARGET_ARMV5
	unsigned int zero = 0;
	ASM("mcr p15, 0, %0, c7, c0, 4" : : "r" (zero) : "memory");
#else
	ASM("wfi":::"memory");
#endif
}

FORCE_INLINE
void KeSpinningHint()
{
	ASM("nop":::"memory");
}

FORCE_INLINE
void KeInvalidatePage(void* Address) {
	uint32_t Zero = 0;
    ASM(
        "mcr p15, 0, %1, c7, c10, 4\n" // data synchronization barrier (DSB)
        "mcr p15, 0, %0, c8, c7, 1\n"
        "mcr p15, 0, %1, c7, c10, 4\n" // DSB
        "mcr p15, 0, %1, c7, c5, 4\n"  // flush prefetch buffer
        :
        : "r"(Address), "r"(Zero)
        : "memory"
    );
}

FORCE_INLINE
uint32_t KiReadIfsr() {
	uint32_t val;
	ASM("mrc p15, 0, %0, c5, c0, 1" : "=r"(val));
	return val;
}

FORCE_INLINE
uint32_t KiReadIfar() {
	uint32_t val;
	ASM("mrc p15, 0, %0, c6, c0, 2" : "=r"(val));
	return val;
}

FORCE_INLINE
uint32_t KiReadDfsr() {
	uint32_t val;
	ASM("mrc p15, 0, %0, c5, c0, 0" : "=r"(val));
	return val;
}

FORCE_INLINE
uint32_t KiReadDfar() {
	uint32_t val;
	ASM("mrc p15, 0, %0, c6, c0, 0" : "=r"(val));
	return val;
}

static_assert(sizeof(int) == 4);

// Special trap numbers:
#define TRAP_CODE_PREFETCH_ABORT  (0x20) // Prefetch Abort means page fault fetching an instruction
#define TRAP_CODE_DATA_ABORT      (0x21) // Data Abort means page fault fetching data or writing to data
#define TRAP_CODE_SYSTEM_SERVICE  (0x22) // System Service (user mode ran "svc")