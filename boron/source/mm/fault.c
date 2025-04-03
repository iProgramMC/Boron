/***
	The Boron Operating System
	Copyright (C) 2023-2025 iProgramInCpp

Module name:
	mm/fault.c
	
Abstract:
	This module contains the memory manager's page
	fault handler.
	
Author:
	iProgramInCpp - 23 September 2023
***/
#include "mi.h"

static BSTATUS MmpHandleFaultCommittedPage(PMMPTE PtePtr)
{
	// This PTE is demand paged.  Allocate a page.
	int Pfn = MmAllocatePhysicalPage();
	
	if (Pfn == PFN_INVALID)
	{
		// TODO: This is probably bad
		return STATUS_REFAULT_SLEEP;
	}
	
	// Create a new, valid, PTE that will replace the current one.
	MMPTE NewPte = *PtePtr;
	NewPte &= ~MM_DPTE_COMMITTED;
	NewPte |=  MM_PTE_PRESENT | MM_PTE_ISFROMPMM;
	NewPte |=  MmPFNToPhysPage(Pfn);
	NewPte &= ~MM_PTE_PKMASK;
	*PtePtr = NewPte;
	
	// Fault was handled successfully.
	return STATUS_SUCCESS;
}

BSTATUS MiNormalFault(UNUSED PEPROCESS Process, UNUSED uintptr_t Va, PMMPTE PtePtr)
{
	// NOTE: IPL is raised to APC level and the relevant address space's lock is held.
	
	// If there is no PTE or it's zero.
	if (!PtePtr || !*PtePtr)
	{
		// Check if there is a VAD with the Committed flag set to 1.
		PMMVAD_LIST VadList = MmLockVadListProcess(PsGetCurrentProcess());
		PMMVAD Vad = MmLookUpVadByAddress(VadList, Va);
		
		if (!Vad)
		{
			// There is no VAD at this address.
			MmUnlockVadList(VadList);
			return STATUS_ACCESS_VIOLATION;
		}
		
		if (!Vad->Flags.Committed)
		{
			// The PTE is NULL yet the VAD isn't fully committed (they didn't request
			// memory with MEM_RESERVE | MEM_COMMIT). Therefore, access violation it is.
			MmUnlockVadList(VadList);
			return STATUS_ACCESS_VIOLATION;
		}
		
		// VAD is committed, so this page fault can be resolved.  But first, try to allocate
		// the PTE itself.
		if (!PtePtr)
		{
			PtePtr = MiGetPTEPointer(MiGetCurrentPageMap(), Va, true);
			
			// If the PTE couldn't be allocated, return with STATUS_REFAULT_SLEEP, waiting for more
			// memory.
			if (!PtePtr)
			{
				MmUnlockVadList(VadList);
				return STATUS_REFAULT_SLEEP;
			}
		}
		
		// Well, the PTE could be allocated, so let's mark it as committed, and continue.
		*PtePtr = Vad->Flags.Protection | MM_DPTE_COMMITTED;
		
		// (Access to the VAD is no longer required now)
		MmUnlockVadList(VadList);
		return MmpHandleFaultCommittedPage(PtePtr);
	}
	
	// There is a PTE pointer, check if it's committed.
	if (PtePtr)
	{
		if (*PtePtr & MM_DPTE_COMMITTED)
			return MmpHandleFaultCommittedPage(PtePtr);
	}
	
	DbgPrint("MiNormalFault: page fault at address %p!", Va);
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
				return STATUS_REFAULT_SLEEP;
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

// Returns: If the page fault failed to be handled, then the reason why.
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
	
	// Attempt to interpret the PTE.
	KIPL OldIpl = MmLockSpaceExclusive(FaultAddress);
	
	PMMPTE PtePtr = MiGetPTEPointer(MiGetCurrentPageMap(), FaultAddress, false);
	
	// If the PTE is present.
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
	
	// The PTE is not present.
	Status = MiNormalFault(Process, FaultAddress, PtePtr);
	
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
		// TODO: Signal to the correct body that they should start trimming working sets
		// and paging stuff out.
		//
		// TODO: Instead of waiting on a timer, perhaps wait on some other dispatcher
		// object that's signaled when there's enough memory.
		//
		// NOTE: This probably sucks and I should really think of a different solution.
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, MI_REFAULT_SLEEP_MS, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE, MODE_KERNEL);
		
		DbgPrint("Out of memory!");
		
		Status = STATUS_REFAULT;
	}
	
	MmUnlockSpace(OldIpl, FaultAddress);
	return Status;
}
