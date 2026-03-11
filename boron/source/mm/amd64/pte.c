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
	return Pte.PteHardware & MM_AMD64_PTE_PRESENT;
}

bool MmIsPoolHeaderPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte) && (Pte.PteHardware & MM_AMD64_DPTE_ISPOOLHDR);
}

bool MmWasPresentPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte)
		&& !(Pte.PteHardware & MM_AMD64_DPTE_ISPOOLHDR)
		&& (Pte.PteHardware & MM_AMD64_DPTE_WASPRESENT);
}

bool MmIsCommittedPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte)
		&& !(Pte.PteHardware & MM_AMD64_DPTE_ISPOOLHDR)
		&& (Pte.PteHardware & MM_AMD64_DPTE_DECOMMITTED);
}

bool MmIsDecommittedPte(MMPTE Pte)
{
	return !MmIsPresentPte(Pte)
		&& !(Pte.PteHardware & MM_AMD64_DPTE_ISPOOLHDR)
		&& (Pte.PteHardware & MM_AMD64_DPTE_COMMITTED);
}

MMPTE MmBuildZeroPte()
{
	MMPTE Pte;
	Pte.PteHardware = 0;
	return Pte;
}

MMPTE MmSetPfnPte(MMPTE Pte, MMPFN NewPfn)
{
	Pte.PteHardware &= ~MM_AMD64_PTE_ADDRESSMASK;
	Pte.PteHardware |= (NewPfn << 12);
	return Pte;
}

MMPTE MmSetPageBitsPte(MMPTE Pte, uintptr_t PageBits)
{
	Pte.PteHardware &= ~(MM_AMD64_PTE_READWRITE
		| MM_AMD64_PTE_USERACCESS
		| MM_AMD64_PTE_NOEXEC
		| MM_AMD64_PTE_CDISABLE
		| MM_AMD64_PTE_ACCESSED
		| MM_AMD64_PTE_DIRTY
		| MM_AMD64_PTE_GLOBAL
		| MM_AMD64_PTE_ISFROMPMM);
	
	if (PageBits & MM_PROT_WRITE)         Pte.PteHardware |= MM_AMD64_PTE_READWRITE;
	if (PageBits & MM_PROT_USER)          Pte.PteHardware |= MM_AMD64_PTE_USERACCESS;
	if (~PageBits & MM_PROT_EXEC)         Pte.PteHardware |= MM_AMD64_PTE_NOEXEC;
	if (PageBits & MM_MISC_DISABLE_CACHE) Pte.PteHardware |= MM_AMD64_PTE_CDISABLE;
	if (PageBits & MM_MISC_ACCESSED)      Pte.PteHardware |= MM_AMD64_PTE_ACCESSED; // do we even need this?
	if (PageBits & MM_MISC_DIRTY)         Pte.PteHardware |= MM_AMD64_PTE_DIRTY;    // do we even need this?
	if (PageBits & MM_MISC_GLOBAL)        Pte.PteHardware |= MM_AMD64_PTE_GLOBAL;
	if (PageBits & MM_MISC_IS_FROM_PMM)   Pte.PteHardware |= MM_AMD64_PTE_ISFROMPMM;
	return Pte;
}

MMPTE MmBuildPte(MMPFN Pfn, uintptr_t PageBits)
{
	MMPTE Pte = MmSetPageBitsPte(MmSetPfnPte(MmBuildZeroPte(), Pfn), PageBits);
	Pte.PteHardware |= MM_AMD64_PTE_PRESENT;
	
	//if (~PageBits & MM_MISC_IS_FROM_PMM) {
	//	DbgPrint("MmBuildPte warning: page potentially declared incorrectly as not from PMM. RA: %p", CallerAddress());
	//}
	//DbgPrint("MmBuildPte(%d, 0x%zx) -> %p. RA: %p", Pfn, PageBits, Pte.PteHardware, CallerAddress());	
	
	return Pte;
}

MMPTE MmBuildWasPresentPte(MMPTE OldPte)
{
	OldPte.PteHardware &= 0x8000FFFFFFFFFFFF;
	OldPte.PteHardware |= MM_AMD64_DPTE_WASPRESENT;
	return OldPte;
}

MMPTE MmBuildAbsentPte(uintptr_t PageBits)
{
	MMPTE Pte = MmBuildZeroPte();
	
	if (PageBits & MM_PAGE_DECOMMITTED)
		Pte.PteHardware |= MM_AMD64_DPTE_DECOMMITTED;
	
	if (PageBits & MM_PAGE_COMMITTED)
	{
		Pte.PteHardware |= MM_AMD64_DPTE_COMMITTED;
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
	Pte.PteHardware = (MMPTE_HW) Address;
	return Pte;
}

uintptr_t MmGetPoolHeaderAddressPte(MMPTE Pte)
{
	if (!MmIsPoolHeaderPte(Pte))
		return 0;
	
	uintptr_t Address = Pte.PteHardware;
	Address &= ~MM_AMD64_DPTE_ISPOOLHDR;
	Address += MM_KERNEL_SPACE_BASE;
	return Address;
}

MMPFN MmGetPfnPte(MMPTE Pte)
{
	return (Pte.PteHardware & MM_AMD64_PTE_ADDRESSMASK) >> 12;
}

uintptr_t MmGetPageBitsPte(MMPTE Pte)
{
	uintptr_t PageBits = MM_PROT_READ | MM_PROT_EXEC;
	
	if (Pte.PteHardware & MM_AMD64_PTE_NOEXEC)     PageBits &= ~MM_PROT_EXEC;
	if (Pte.PteHardware & MM_AMD64_PTE_READWRITE)  PageBits |= MM_PROT_WRITE;
	if (Pte.PteHardware & MM_AMD64_PTE_USERACCESS) PageBits |= MM_PROT_USER;
	if (Pte.PteHardware & MM_AMD64_PTE_ACCESSED)   PageBits |= MM_MISC_ACCESSED;
	if (Pte.PteHardware & MM_AMD64_PTE_DIRTY)      PageBits |= MM_MISC_DIRTY;
	if (Pte.PteHardware & MM_AMD64_PTE_CDISABLE)   PageBits |= MM_MISC_DISABLE_CACHE;
	if (Pte.PteHardware & MM_AMD64_PTE_GLOBAL)     PageBits |= MM_MISC_GLOBAL;
	if (Pte.PteHardware & MM_AMD64_PTE_ISFROMPMM)  PageBits |= MM_MISC_IS_FROM_PMM;
	
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
	
	if (Pte.PteHardware & MM_AMD64_PTE_PAGESIZE)
		return true;
	
	// other unsupported properties here...?
	
	return false;
}

bool MmIsFromPmmPte(MMPTE Pte)
{
	return (MmIsPresentPte(Pte) || MmWasPresentPte(Pte)) && (Pte.PteHardware & MM_AMD64_PTE_ISFROMPMM);
}
