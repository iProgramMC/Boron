/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/fault.c
	
Abstract:
	This module contains the memory manager's page
	fault handler.
	
Author:
	iProgramInCpp - 23 September 2023
***/
#include <mm.h>
#include <ke.h>

// TODO: Can't allow recursive page fault in kernel space. Need to fix that!

// Returns: Whether the page fault was fixed or not
BSTATUS MmPageFault(UNUSED uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode)
{
	bool IsKernelSpace = false;
	
#ifdef DEBUG2
	DbgPrint("Handling page fault at PC=%p ADDR=%p MODE=%p", FaultPC, FaultAddress, FaultMode);
#endif
	
	if (FaultAddress >= MM_KERNEL_SPACE_BASE)
		IsKernelSpace = true;
	
	if (KeGetIPL() >= IPL_DPC)
	{
		// Page faults may only be taken at IPL_APC and lower, to prevent deadlock
		// and data corruption.
		
		// So we don't handle the page fault, just return immediately.
		
		return STATUS_IPL_TOO_HIGH;
	}
	
	// However, page faults aren't handled at IPL_DPC, but at the IPL of the thread.
	// Think of a page fault like a sort of forced, and often unexpected, system call.
	
	// Check the fault reason.
	if (FaultMode & MM_FAULT_PROTECTION)
	{
		// The page is present but we aren't allowed to touch it currently.
		
		// What's handled here:
		// - CoW (copy-on-write)
		// - Shared mappings that get demoted to private when written (such as when loading a relocatable dll)
		
		// TODO
		
		return STATUS_UNIMPLEMENTED;
	}
	else
	{
		MmLockSpaceExclusive(FaultAddress);
		
		// The PTE was not marked present. Let's see what we're dealing with here
		PMMPTE Pte = MiGetPTEPointer(MiGetCurrentPageMap(), FaultAddress, false);
		
		// If we don't have a PTE here, that means that we didn't mess with anything around there,
		// thus we should return..
		if (!Pte)
		{
			MmUnlockSpace(FaultAddress);
			return STATUS_ACCESS_VIOLATION;
		}
		
		// Is the PTE demand paged?
		if (*Pte & MM_DPTE_DEMANDPAGED)
		{
			// Okay! Let's allocate a page.
			int Pfn = MmAllocatePhysicalPage();
			
			if (Pfn == PFN_INVALID)
			{
				// Error: Out of memory! This is bad, but we can check if we can do anything to fix it.
				// TODO
				DbgPrint("ERROR! Out of memory trying to handle page fault at %p (mode %d) at PC=%p", FaultAddress, FaultMode, FaultPC);
				
				MmUnlockSpace(FaultAddress);
				
				return STATUS_INSUFFICIENT_MEMORY;
			}
			
			// Create a new, valid, PTE that will replace the current one.
			MMPTE NewPte = *Pte;
			NewPte &= ~MM_DPTE_DEMANDPAGED;
			NewPte |=  MM_PTE_PRESENT | MM_PTE_ISFROMPMM;
			NewPte |=  MmPFNToPhysPage(Pfn);
			NewPte &= ~MM_PTE_PKMASK;
			*Pte = NewPte;
			
			MmUnlockSpace(FaultAddress);
			
			// Fault was handled successfully.
			return STATUS_SUCCESS;
		}
		
		if (IsKernelSpace)
			MmUnlockSpace(FaultAddress);
		
		// TODO
		return STATUS_UNIMPLEMENTED;
	}
}
