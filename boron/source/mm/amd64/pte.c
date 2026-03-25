/***
	The Boron Operating System
	Copyright (C) 2026 iProgramInCpp

Module name:
	mm/amd64/pte.c
	
Abstract:
	This module implements page table entry management
	functions for the AMD64 platform.
	
Author:
	iProgramInCpp - 11 March 2026
***/
#include <mm.h>
#include <arch.h>

bool MmIsPresentPte(MMPTE Pte)
{
	return MmHardwarePte(Pte) & MM_AMD64_PTE_PRESENT;
}

bool MmIsPoolHeaderPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte) && (MmHardwarePte(Pte) & MM_AMD64_DPTE_ISPOOLHDR);
}

bool MmWasPresentPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte)
		&& !(MmHardwarePte(Pte) & MM_AMD64_DPTE_ISPOOLHDR)
		&& (MmHardwarePte(Pte) & MM_AMD64_DPTE_WASPRESENT);
}

bool MmIsCommittedPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte)
		&& !(MmHardwarePte(Pte) & MM_AMD64_DPTE_ISPOOLHDR)
		&& (MmHardwarePte(Pte) & MM_AMD64_DPTE_DECOMMITTED);
}

bool MmIsDecommittedPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte)
		&& !(MmHardwarePte(Pte) & MM_AMD64_DPTE_ISPOOLHDR)
		&& (MmHardwarePte(Pte) & MM_AMD64_DPTE_COMMITTED);
}

MMPTE MmBuildZeroPte()
{
	MMPTE Pte;
	MmHardwarePte(Pte) = 0;
	return Pte;
}

MMPTE MmSetPfnPte(MMPTE Pte, MMPFN NewPfn)
{
	MmHardwarePte(Pte) &= ~MM_AMD64_PTE_ADDRESSMASK;
	MmHardwarePte(Pte) |= (NewPfn << 12);
	return Pte;
}

MMPTE MmSetPageBitsPte(MMPTE Pte, uintptr_t PageBits)
{
	MmHardwarePte(Pte) &= ~(MM_AMD64_PTE_READWRITE
		| MM_AMD64_PTE_USERACCESS
		| MM_AMD64_PTE_NOEXEC
		| MM_AMD64_PTE_CDISABLE
		| MM_AMD64_PTE_ACCESSED
		| MM_AMD64_PTE_DIRTY
		| MM_AMD64_PTE_GLOBAL
		| MM_AMD64_PTE_ISFROMPMM);
	
	if (PageBits & MM_PROT_WRITE)         MmHardwarePte(Pte) |= MM_AMD64_PTE_READWRITE;
	if (PageBits & MM_PROT_USER)          MmHardwarePte(Pte) |= MM_AMD64_PTE_USERACCESS;
	if (~PageBits & MM_PROT_EXEC)         MmHardwarePte(Pte) |= MM_AMD64_PTE_NOEXEC;
	if (PageBits & MM_MISC_DISABLE_CACHE) MmHardwarePte(Pte) |= MM_AMD64_PTE_CDISABLE;
	if (PageBits & MM_MISC_ACCESSED)      MmHardwarePte(Pte) |= MM_AMD64_PTE_ACCESSED; // do we even need this?
	if (PageBits & MM_MISC_DIRTY)         MmHardwarePte(Pte) |= MM_AMD64_PTE_DIRTY;    // do we even need this?
	if (PageBits & MM_MISC_GLOBAL)        MmHardwarePte(Pte) |= MM_AMD64_PTE_GLOBAL;
	if (PageBits & MM_MISC_IS_FROM_PMM)   MmHardwarePte(Pte) |= MM_AMD64_PTE_ISFROMPMM;
	return Pte;
}

MMPTE MmBuildPte(MMPFN Pfn, uintptr_t PageBits)
{
	MMPTE Pte = MmSetPageBitsPte(MmSetPfnPte(MmBuildZeroPte(), Pfn), PageBits);
	MmHardwarePte(Pte) |= MM_AMD64_PTE_PRESENT;
	
	//if (~PageBits & MM_MISC_IS_FROM_PMM) {
	//	DbgPrint("MmBuildPte warning: page potentially declared incorrectly as not from PMM. RA: %p", CallerAddress());
	//}
	//DbgPrint("MmBuildPte(%d, 0x%zx) -> %p. RA: %p", Pfn, PageBits, MmHardwarePte(Pte), CallerAddress());	
	
	return Pte;
}

MMPTE MmBuildWasPresentPte(MMPTE OldPte)
{
	MmHardwarePte(OldPte) &= 0x8000FFFFFFFFFFFF;
	MmHardwarePte(OldPte) |= MM_AMD64_DPTE_WASPRESENT;
	return OldPte;
}

MMPTE MmBuildAbsentPte(uintptr_t PageBits)
{
	MMPTE Pte = MmBuildZeroPte();
	
	if (PageBits & MM_PAGE_DECOMMITTED)
		MmHardwarePte(Pte) |= MM_AMD64_DPTE_DECOMMITTED;
	
	if (PageBits & MM_PAGE_COMMITTED)
	{
		MmHardwarePte(Pte) |= MM_AMD64_DPTE_COMMITTED;
		/*
		uintptr_t PermBits = PageBits & (MM_PROT_READ
			| MM_PROT_WRITE
			| MM_PROT_EXEC
			| MM_PROT_USER
			| MM_MISC_DISABLE_CACHE
			| MM_MISC_IS_FROM_PMM
			| MM_MISC_GLOBAL);
		
		PageBits &= PermBits;
		Pte = MmSetPageBitsPte(Pte, PageBits);*/
	}
	
	return Pte;
}

MMPTE MmBuildPoolHeaderPte(uintptr_t Address)
{
	Address -= MM_KERNEL_SPACE_BASE;
	Address |= MM_AMD64_DPTE_ISPOOLHDR;
	MMPTE Pte;
	MmHardwarePte(Pte) = (MMPTE_HW) Address;
	return Pte;
}

uintptr_t MmGetPoolHeaderAddressPte(MMPTE Pte)
{
	if (!MmIsPoolHeaderPte(Pte))
		return 0;
	
	uintptr_t Address = MmHardwarePte(Pte);
	Address &= ~MM_AMD64_DPTE_ISPOOLHDR;
	Address += MM_KERNEL_SPACE_BASE;
	return Address;
}

MMPFN MmGetPfnPte(MMPTE Pte)
{
	return (MmHardwarePte(Pte) & MM_AMD64_PTE_ADDRESSMASK) >> 12;
}

uintptr_t MmGetPageBitsPte(MMPTE Pte)
{
	uintptr_t PageBits = MM_PROT_READ | MM_PROT_EXEC;
	
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_NOEXEC)     PageBits &= ~MM_PROT_EXEC;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_READWRITE)  PageBits |= MM_PROT_WRITE;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_USERACCESS) PageBits |= MM_PROT_USER;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_ACCESSED)   PageBits |= MM_MISC_ACCESSED;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_DIRTY)      PageBits |= MM_MISC_DIRTY;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_CDISABLE)   PageBits |= MM_MISC_DISABLE_CACHE;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_GLOBAL)     PageBits |= MM_MISC_GLOBAL;
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_ISFROMPMM)  PageBits |= MM_MISC_IS_FROM_PMM;
	
	return PageBits;
}

bool MmIsEqualPte(MMPTE Pte1, MMPTE Pte2)
{
	return Pte1.PteHardware == Pte2.PteHardware;
}

bool MmIsUnsupportedHigherLevelPte(MMPTE Pte)
{
	if (!MmIsPresentPte(Pte))
		return false;
	
	if (MmHardwarePte(Pte) & MM_AMD64_PTE_PAGESIZE)
		return true;
	
	// other unsupported properties here...?
	
	return false;
}

bool MmIsFromPmmPte(MMPTE Pte)
{
	return (MmIsPresentPte(Pte) || MmWasPresentPte(Pte)) && (MmHardwarePte(Pte) & MM_AMD64_PTE_ISFROMPMM);
}

void MmFlushTlbUpdates()
{
	// On amd64, the TLB is coherent against the data cache, so no need for anything
}
