/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/arm/pte.c
	
Abstract:
	This module implements page table entry management
	functions for the ARM platform.
	
Author:
	iProgramInCpp - 14 March 2026
***/
#include <mm.h>
#include <arch.h>

bool MmIsPresentPte(MMPTE Pte)
{
	return (MmHardwarePte(Pte) & MM_ARM_PTEL1_TYPE) != 0;
}

bool MmIsPoolHeaderPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte) && (MmHardwarePte(Pte) & MM_ARM_DPTE_ISPOOLHDR);
}

bool MmWasPresentPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte) && (MmHardwarePte(Pte) & MM_ARM_DPTE_WASPRESENT);
}

bool MmIsCommittedPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte) && (MmHardwarePte(Pte) & MM_ARM_DPTE_COMMITTED);
}

bool MmIsDecommittedPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte) && (MmHardwarePte(Pte) & MM_ARM_DPTE_DECOMMITTED);
}

MMPTE MmBuildZeroPte()
{
	MMPTE Pte;
	MmHardwarePte(Pte) = 0;
	return Pte;
}

MMPTE MmSetPfnPte(MMPTE Pte, MMPFN NewPfn)
{
	MmHardwarePte(Pte) &= ~MM_ARM_PTE_ADDRESSMASK;
	MmHardwarePte(Pte) |= MM_ARM_PTE_NEWPFN(NewPfn);
	return Pte;
}

MMPTE MmSetPageBitsPte(MMPTE Pte, uintptr_t PageBits)
{
/*
	MmHardwarePte(Pte) &= ~(MM_I386_PTE_READWRITE
		| MM_I386_PTE_USERACCESS
		| MM_I386_PTE_CDISABLE
		| MM_I386_PTE_ACCESSED
		| MM_I386_PTE_DIRTY
		| MM_I386_PTE_ISFROMPMM);
	
	if (PageBits & MM_PROT_WRITE)         MmHardwarePte(Pte) |= MM_I386_PTE_READWRITE;
	if (PageBits & MM_PROT_USER)          MmHardwarePte(Pte) |= MM_I386_PTE_USERACCESS;
	if (PageBits & MM_MISC_DISABLE_CACHE) MmHardwarePte(Pte) |= MM_I386_PTE_CDISABLE;
	if (PageBits & MM_MISC_ACCESSED)      MmHardwarePte(Pte) |= MM_I386_PTE_ACCESSED; // do we even need this?
	if (PageBits & MM_MISC_DIRTY)         MmHardwarePte(Pte) |= MM_I386_PTE_DIRTY;    // do we even need this?
	if (PageBits & MM_MISC_IS_FROM_PMM)   MmHardwarePte(Pte) |= MM_I386_PTE_ISFROMPMM;
	return Pte;
*/

#ifdef TARGET_ARMV6
	bool IsPresent = MmIsPresentPte(Pte);
	
	MmHardwarePte(Pte) &= ~(MM_ARM_AP_MASK // remove permissions
		| MM_ARM_PTEL2_TEX_MASK            // remove caching information
		| MM_ARM_PTEL2_CB_MASK
		| MM_ARM_PTEL2_TYPE_SMALLPAGENX    // actually removes ALL presence info
	);
	
	uintptr_t WU = PageBits & (MM_PROT_WRITE | MM_PROT_USER);
	switch (WU)
	{
		case MM_PROT_WRITE | MM_PROT_USER:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_USERREADWRITE;
			break;
		case MM_PROT_WRITE:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_SUPERREADWRITE;
			break;
		case MM_PROT_USER:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_BOTHREADONLY;
			break;
		case 0:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_SUPERREADONLY;
			break;
		default:
			ASSERT(!"Unreachable");
			break;
	}
	
	if (IsPresent)
	{
		if (PageBits & MM_PROT_EXEC)
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_TYPE_SMALLPAGE;
		else
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_TYPE_SMALLPAGENX;
	}
	
	if (~PageBits & MM_MISC_DISABLE_CACHE)
	{
		MmHardwarePte(Pte) |= MM_ARM_PTEL2_TEXCB_NORMALMEM;
	}
#else
	bool IsPresent = MmIsPresentPte(Pte);
	
	MmHardwarePte(Pte) &= ~(MM_ARM_AP_MASK // remove permissions
		| MM_ARM_PTEL2_B
		| MM_ARM_PTEL2_C
		| MM_ARM_PTEL2_TYPE_EXSMALLPAGE    // actually removes ALL presence info
	);
	
	uintptr_t WU = PageBits & (MM_PROT_WRITE | MM_PROT_USER);
	switch (WU)
	{
		case MM_PROT_WRITE | MM_PROT_USER:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_ALL_USERREADWRITE;
			break;
		case MM_PROT_WRITE:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_ALL_SUPERREADWRITE;
			break;
		// bummer. Superuser cannot opt out of writes!
		// reminds me of the 80386 which also doesn't have the concept
		// of pages read only for kernel mode in addition to user mode.
		case MM_PROT_USER:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_ALL_USERREADONLY;
			break;
		case 0:
			MmHardwarePte(Pte) |= MM_ARM_PTEL2_AP_ALL_SUPERREADWRITE;
			break;
		default:
			ASSERT(!"Unreachable");
			break;
	}
	
	if (IsPresent)
	{
		MmHardwarePte(Pte) |= MM_ARM_PTEL2_TYPE_SMALLPAGE;
	}
	
	if (~PageBits & MM_MISC_DISABLE_CACHE)
	{
		MmHardwarePte(Pte) |= MM_ARM_PTEL2_C | MM_ARM_PTEL2_B;
	}
#endif

	// N.B. Ignoring MM_MISC_IS_FROM_PMM because arm has no such concept as "software-use PTE bits".
	
	return Pte;
}

MMPTE MmBuildPte(MMPFN Pfn, uintptr_t PageBits)
{
	MMPTE Pte = MmBuildZeroPte();
	MmHardwarePte(Pte) |= MM_ARM_PTEL2_TYPE_SMALLPAGE;
	
	Pte = MmSetPfnPte(Pte, Pfn);
	Pte = MmSetPageBitsPte(Pte, PageBits);
	
	return Pte;
}

MMPTE MmBuildWasPresentPte(MMPTE OldPte)
{
	// not needed on ARM
	(void) OldPte;
	return MmBuildZeroPte();
}

MMPTE MmBuildAbsentPte(uintptr_t PageBits)
{
	MMPTE Pte = MmBuildZeroPte();
	
	if (PageBits & MM_PAGE_DECOMMITTED)
		MmHardwarePte(Pte) |= MM_ARM_DPTE_DECOMMITTED;
	
	if (PageBits & MM_PAGE_COMMITTED)
		MmHardwarePte(Pte) |= MM_ARM_DPTE_COMMITTED;
	
	return Pte;
}

// the structure of the pool header PTE if this is set is as follows:
//
// Address[30:3] 0 1 0 0
//
// - bits 0 and 1 are cleared because ARM uses the first 2 bits as the PTE's "type"
// - bit 2 is set because that's MM_DPTE_ISPOOLHDR
// - bit 3 is cleared because that's MM_DPTE_COMMITTED and it shouldn't conflict
typedef union
{
	MMPTE_HW Pte;
	
	struct
	{
		uintptr_t Present   : 2; // MUST be zero
		uintptr_t IsPoolHdr : 1; // MUST be ONE
		uintptr_t Committed : 1; // MUST be zero
		uintptr_t B3to30    : 28;
	}
	PACKED;
}
MMPTE_POOLHEADER;

static_assert(sizeof(MMPTE_POOLHEADER) == sizeof(uint32_t));
static_assert(MM_ARM_DPTE_ISPOOLHDR == (1 << 2));
static_assert(MM_ARM_DPTE_COMMITTED == (1 << 3));

MMPTE MmBuildPoolHeaderPte(uintptr_t Handle)
{
	ASSERT(!(Handle & 0x7));
	MMPTE_POOLHEADER PteHeader;
	PteHeader.Pte = 0;
	
	PteHeader.B3to30 = Handle >> 3;
	PteHeader.IsPoolHdr = true;

	MMPTE ActualPte;
	MmHardwarePte(ActualPte) = PteHeader.Pte;
	return ActualPte;
}

uintptr_t MmGetPoolHeaderAddressPte(MMPTE Pte)
{
	MMPTE_POOLHEADER PteHeader;
	PteHeader.Pte = MmHardwarePte(Pte);
	
	ASSERT(!PteHeader.Present);
	ASSERT(!PteHeader.Committed);
	ASSERT(PteHeader.IsPoolHdr);
	
	return (PteHeader.B3to30 << 3) | 0x80000000;
}

MMPFN MmGetPfnPte(MMPTE Pte)
{
	return MM_ARM_PTE_PFN(MmHardwarePte(Pte));
}

uintptr_t MmGetPageBitsPte(MMPTE Pte)
{
	uintptr_t PageBits = MM_PROT_READ;
	
#ifdef TARGET_ARMV5
	PageBits |= MM_PROT_EXEC;
	
	if ((MmHardwarePte(Pte) & (MM_ARM_PTEL2_C | MM_ARM_PTEL2_B)) == 0)
		PageBits |= MM_MISC_DISABLE_CACHE;
#else
	if ((MmHardwarePte(Pte) & (MM_ARM_PTEL2_TEX_MASK | MM_ARM_PTEL2_CB_MASK)) == 0)
		PageBits |= MM_MISC_DISABLE_CACHE;
#endif
	
	switch (MmHardwarePte(Pte) & MM_ARM_AP_MASK)
	{
		case 0:
			PageBits &= ~MM_PROT_READ;
			break;
		case MM_ARM_PTEL2_AP_SUPERREADWRITE:
			PageBits |= MM_PROT_WRITE;
			break;
		case MM_ARM_PTEL2_AP_USERREADONLY:
			PageBits |= MM_PROT_USER;
			break;
		case MM_ARM_PTEL2_AP_USERREADWRITE:
			PageBits |= MM_PROT_USER | MM_PROT_WRITE;
			break;
	#ifndef TARGET_ARMV5
		case MM_ARM_PTEL2_AP_SUPERREADONLY:
			break;
		case MM_ARM_PTEL2_AP_BOTHREADONLY:
			PageBits |= MM_PROT_USER;
			break;
	#endif
		default:
			KeCrash("Unknown protection bits in PTE %p", MmHardwarePte(Pte));
			break;
	}
	
	// ARM has no software-defined bits... so just tell whoever's calling that
	// this page came from the PMM, and be done with it.
	PageBits |= MM_MISC_IS_FROM_PMM;
	
	return PageBits;
}

bool MmIsEqualPte(MMPTE Pte1, MMPTE Pte2)
{
	return MmHardwarePte(Pte1) == MmHardwarePte(Pte2);
}

bool MmIsUnsupportedHigherLevelPte(MMPTE Pte)
{
	if (!MmIsPresentPte(Pte))
		return false;
	
	// other unsupported properties here...?
	
	return false;
}

bool MmIsFromPmmPte(MMPTE Pte)
{
	// Same as MmGetPageBitsPte()...
	(void) Pte;
	return true;
}
