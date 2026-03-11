/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/pte.h
	
Abstract:
	This header file contains the definitions for the page table
	entry builder.  This builds page table entries (PTEs) compatible
	with the hardware Boron is running on.
	
Author:
	iProgramInCpp - 10 March 2026
***/
#pragma once

#include "pfn.h"

typedef struct {
	MMPTE_HW PteHardware;
}
MMPTE, *PMMPTE;

//
// Protection bits.
//
// MM_PROT_READ - Whether the page can be read from.  This is not supported
//                on i386 and amd64, because every present page can be read.
//
// MM_PROT_WRITE- Whether the page can be written to.
//                On the 80386, the R/W PTE field is ignored in ring 0.
//                However, currently we target 80486 and up, so we can ignore
//                this quirk for now.
//
// MM_PROT_EXEC - Whether the page can be read from for program execution.
//                This is not supported on i386.
//
// MM_PROT_USER - Whether user mode has access to this page of memory.
//
#define MM_PROT_READ  (1 << 0)
#define MM_PROT_WRITE (1 << 1)
#define MM_PROT_EXEC  (1 << 2)
#define MM_PROT_USER  (1 << 3)

//
// Misc bits.
//
// MM_MISC_ACCESSED      - Whether the page for this PTE was accessed.
//
// MM_MISC_DIRTY         - Whether the page for this PTE was written to.
//
// MM_MISC_DISABLE_CACHE - Totally disable caching.
//
// MM_MISC_GLOBAL        - This PTE will not be invalidated on a page table switch, if supported.
//
// MM_MISC_IS_FROM_PMM   - The specified PTE has been created from a PFN provided by the PMM.
//
#define MM_MISC_ACCESSED      (1 << 4)
#define MM_MISC_DIRTY         (1 << 5)
#define MM_MISC_DISABLE_CACHE (1 << 6)
#define MM_MISC_IS_FROM_PMM   (1 << 7)
#define MM_MISC_GLOBAL        (1 << 8)

//
// Absent PTE bits.
//
// MM_PAGE_DECOMMITTED  - If this page was previously committed but is no longer committed.
//
// MM_PAGE_COMMITTED    - If this page was committed but a physical page was not allocated yet.
//
#define MM_PAGE_DECOMMITTED  (1 << 0)
#define MM_PAGE_COMMITTED    (1 << 1)

//
// These functions are meant to be implemented in the platform compatibility
// layer (e.g. /source/mm/amd64/pt.c).
//

// Builds a zero PTE.
MMPTE MmBuildZeroPte();

// Builds a present PTE with the specified PFN and page bits.
MMPTE MmBuildPte(MMPFN Pfn, uintptr_t PageBits);

// Builds a "was present" PTE. This is used in the process of unmapping pages.
MMPTE MmBuildWasPresentPte(MMPTE OldPte);

// Builds an absent PTE with the specified bits.
MMPTE MmBuildAbsentPte(uintptr_t PageBits);

// Builds a pool header PTE.
MMPTE MmBuildPoolHeaderPte(uintptr_t PoolHeaderAddress);

// Updates the PFN of the specified PTE.
MMPTE MmSetPfnPte(MMPTE Pte, MMPFN NewPfn);

// Updates the page bits of the specified PTE.
MMPTE MmSetPageBitsPte(MMPTE Pte, uintptr_t PageBits);

// Obtains the PFN from a PTE.
MMPFN MmGetPfnPte(MMPTE Pte);

// Obtains the page bits from a present PTE.
uintptr_t MmGetPageBitsPte(MMPTE Pte);

// Checks if the PTE is present.
bool MmIsPresentPte(MMPTE Pte);

// Checks if the PTE was present.
bool MmWasPresentPte(MMPTE Pte);

// Checks if the PTE references a page from the PMM.
bool MmIsFromPmmPte(MMPTE Pte);

// Checks if the PTE is a pool header PTE.
bool MmIsPoolHeaderPte(MMPTE Pte);

// Get the address of the pool header that this PTE references.
uintptr_t MmGetPoolHeaderAddressPte(MMPTE Pte);

// Checkss if the PTE is committed.
bool MmIsCommittedPte(MMPTE Pte);

// Checks if the PTE is decommitted.
bool MmIsDecommittedPte(MMPTE Pte);

// Checks if two PTEs are equal.
bool MmIsEqualPte(MMPTE Pte1, MMPTE Pte2);

// Checks if the PTE has unsupported parameters.
bool MmIsUnsupportedHigherLevelPte(MMPTE Pte);
