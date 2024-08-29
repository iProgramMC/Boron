/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	mm/fault.c
	
Abstract:
	This module contains the memory manager's page
	fault handler.
	
Author:
	iProgramInCpp - 23 September 2023
***/
#include "mi.h"

extern EPROCESS PsSystemProcess;

BSTATUS MiNormalFault(UNUSED PEPROCESS Process, UNUSED uintptr_t Va)
{
	// NOTE: IPL is raised to APC level and the relevant address space's lock is held.
	
	// TODO
	return STATUS_ACCESS_VIOLATION;
}

BSTATUS MiWriteFault(UNUSED PEPROCESS Process, uintptr_t Va, PMMPTE PtePtr)
{
	// This is a write fault:
	//
	// There are two types:
	// - Copy on write fault
	// - Access violation
	//
	// NOTE: IPL is raised to APC level and the relevant address space's lock is held.
	
	if (*PtePtr & MM_PTE_COW)
	{
		// Lock the PFDB to check the reference count of the page.
		//
		// The page will only be copied if the page has a reference count of more than one.
		KIPL Ipl = MiLockPfdb();
		
		MMPFN Pfn = (*PtePtr & MM_PTE_ADDRESSMASK) / PAGE_SIZE;
		
		int RefCount = MiGetReferenceCountPfn(Pfn);
		ASSERT(RefCount > 0);
		
		if (RefCount == 1)
		{
			// Simply make the page writable.
			*PtePtr = (*PtePtr & ~MM_PTE_COW) | MM_PTE_READWRITE;
			MiUnlockPfdb(Ipl);
		}
		else
		{
			// The page has to be duplicated.
			MiUnlockPfdb(Ipl);
			
			MMPFN NewPfn = MmAllocatePhysicalPage();
			if (NewPfn == PFN_INVALID)
			{
				// TODO: This is probably bad
				return STATUS_INSUFFICIENT_MEMORY;
			}
			
			void* Address = MmGetHHDMOffsetAddr(MmPFNToPhysPage(NewPfn));
			memcpy(Address, (void*) Va, PAGE_SIZE);
			
			// Now assign the new PFN.
			*PtePtr =
				(*PtePtr & ~(MM_PTE_COW | MM_PTE_ADDRESSMASK)) |
				MM_PTE_READWRITE |
				(NewPfn * PAGE_SIZE);
			
			// Finally, free the reference to the old page frame.
			MmFreePhysicalPage(Pfn);
		}
		
		return STATUS_SUCCESS;
	}
	
	return STATUS_ACCESS_VIOLATION;
}

// Returns: Whether the page fault was fixed or not
BSTATUS MmPageFault(UNUSED uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode)
{
	UNUSED bool IsKernelSpace = false;
	BSTATUS Status = STATUS_SUCCESS;
	PEPROCESS Process = PsGetCurrentProcess();
	
	if (FaultAddress >= MM_KERNEL_SPACE_BASE)
	{
		IsKernelSpace = true;
		Process = &PsSystemProcess;
	}
	
	// TODO: There may be conditions where accesses to kernel space are done on
	// behalf of a user process.
	
	// Avoid recursive page faults by raising IPL to IPL_APC.  APCs may take page faults too.
	KIPL Ipl = KeRaiseIPL(IPL_APC);
	
	// Attempt to interpret the PTE.
	MmLockSpaceExclusive(FaultAddress);
	
	PMMPTE PtePtr = MiGetPTEPointer(MiGetCurrentPageMap(), FaultAddress, false);
	
	if (PtePtr && (*PtePtr & MM_PTE_PRESENT))
	{
		// The PTE exists and is present.
		if (*PtePtr & MM_PTE_READWRITE)
		{
			// The PTE is writable already, therefore this fault may have been spurious
			// and we should simply return.
			Status = STATUS_SUCCESS;
			goto EarlyExit;
		}
		
		if (FaultMode & MM_FAULT_WRITE)
		{
			// A write fault occurred on a readonly page.
			Status = MiWriteFault(Process, FaultAddress, PtePtr);
			goto EarlyExit;
		}
		
		// The PTE is valid, and this wasn't a write fault, so this fault may have been
		// spurious and we should simply return.
		Status = STATUS_SUCCESS;
		goto EarlyExit;
	}
	
	if (!PtePtr)
	{
		// TODO: In which situations would the PTE pointer not exist?
	}
	else
	{
		// The PTE exists but it's not marked present.  Figure out why that is.
		//
		// If the PTE is zero, handle the fault normally.  If it's not zero,
		// we don't know what to do right now.  But it'll be figured out later!
		if (*PtePtr)
		{
			// TODO
		}
	}
	
	Status = MiNormalFault(Process, FaultAddress);
	
	if (SUCCEEDED(Status) && (FaultMode & MM_FAULT_WRITE))
	{
		// The PTE was made valid, but it might still be readonly.  Refault
		// to make the page writable.  Note that returning STATUS_REFAULT
		// simply brings us back into MmPageFault instead of going all the
		// way back to the offending process to save some cycles.
		Status = STATUS_REFAULT;
	}
	
EarlyExit:
	if (Status == STATUS_REFAULT_SLEEP)
	{
		// The page fault could not be serviced due to an out of memory condition.
		// It will be retried in a few milliseconds.
		//
		// NOTE: This probably sucks and I should really think of a different solution.
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, MI_REFAULT_SLEEP_MS, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
		
		Status = STATUS_REFAULT;
	}
	
	MmUnlockSpace(FaultAddress);
	KeLowerIPL(Ipl);
	return Status;
	
	// This code is still useful to see what the old boron page fault handler did
#if 0
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
#endif
}
