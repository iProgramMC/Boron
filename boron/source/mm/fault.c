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
#include <io.h>

//#define PAGE_FAULT_DEBUG

#ifdef PAGE_FAULT_DEBUG
#define PFDbgPrint(...) DbgPrint(__VA_ARGS__)
#else
#define PFDbgPrint(...)
#endif

static PMMVAD_LIST MmpLockVadListByAddress(PEPROCESS Process, uintptr_t Va)
{
	bool IsViewSpace = Va >= MM_KERNEL_SPACE_BASE;
	PMMVAD_LIST VadList;

	if (IsViewSpace)
	{
		MiLockVadList(&MiSystemVadList);
		VadList = &MiSystemVadList;
	}
	else
	{
		VadList = MmLockVadListProcess(Process);
	}
	
	return VadList;
}

static BSTATUS MmpHandleFaultCommittedPage(PMMPTE PtePtr, uintptr_t PageBits)
{
	// This PTE is demand paged.  Allocate a page.
	MMPFN Pfn = MmAllocatePhysicalPage();
	
	if (Pfn == PFN_INVALID)
	{
		// TODO: This is probably bad
		return STATUS_REFAULT_SLEEP;
	}
	
	*PtePtr = MmBuildPte(Pfn, PageBits | MM_MISC_IS_FROM_PMM);
	MmFlushTlbUpdates();
	return STATUS_SUCCESS;
}

static BSTATUS MmpAssignPfnToAddress(uintptr_t Va, MMPFN Pfn, int Protection)
{
	uintptr_t PageBits = MM_PROT_READ | MM_MISC_IS_FROM_PMM;
	
	if (Va < MM_KERNEL_SPACE_BASE)
		PageBits |= MM_PROT_USER;
	
	PMMPTE PtePtr = MmGetPteLocationCheck(Va, true);
	if (!PtePtr)
		return STATUS_INSUFFICIENT_MEMORY;
	
	*PtePtr = MmBuildPte(Pfn, PageBits | MmGetPteBitsFromProtection(Protection));
	MmFlushTlbUpdates();
	return STATUS_SUCCESS;
}

// NOTE: Even if at the start of this function, the address space lock is held,
// by the end of this function, the VAD list and address space lock need to be
// released.
BSTATUS MiNormalFault(PEPROCESS Process, uintptr_t Va, PMMPTE PtePtr, KIPL SpaceUnlockIpl, bool IsInstructionFetchFault, bool* RefaultForWrite)
{
	// NOTE: IPL is raised to APC level and the relevant address space's lock is held.
	BSTATUS Status;
	
	// Check if there is a VAD with the Committed flag set to 1.
	PMMVAD_LIST VadList = MmpLockVadListByAddress(Process, Va);
	PMMVAD Vad = MmLookUpVadByAddress(VadList, Va);
	
	if (!Vad)
	{
		// There is no VAD at this address.
		MmUnlockVadList(VadList);
		MmUnlockSpace(SpaceUnlockIpl, Va);
		
		// However, check if this is a demand-page pool address.
		uintptr_t PoolStart = MiGetTopOfPoolManagedArea();
		uintptr_t PoolEnd = PoolStart + (1ULL << MI_POOL_LOG2_SIZE);
		
	#ifdef MI_USE_TWO_POOLS
		uintptr_t Pool2Start = MiGetTopOfSecondPoolManagedArea();
		uintptr_t Pool2End = PoolStart + (1ULL << MI_POOL_LOG2_SIZE_2ND);
	#endif
		
		if ((PoolStart <= Va && Va < PoolEnd)
		#ifdef MI_USE_TWO_POOLS
			|| (Pool2Start <= Va && Va < Pool2End)
		#endif
			)
		{
			// Is in a pool area, so allocate a page of memory and map it there.
			PMMPTE PtePtr = MmGetPteLocationCheck(Va, true);
			if (MmIsCommittedPte(*PtePtr))
			{
				*RefaultForWrite = false;
				return MmpHandleFaultCommittedPage(PtePtr, MM_PROT_READ | MM_PROT_WRITE);
			}
		}
		
		DbgPrint("MiNormalFault: Declaring access violation on VA %p. No VAD and not in a pool area", Va);
		return STATUS_ACCESS_VIOLATION;
	}
	
	// Check if the actual PTE is even there.
	if (!PtePtr)
	{
		PtePtr = MmGetPteLocationCheck(Va, true);
		if (!PtePtr)
		{
			// PTE couldn't be allocated.
			// Wait for more memory.
			MmUnlockVadList(VadList);
			MmUnlockSpace(SpaceUnlockIpl, Va);
			return STATUS_REFAULT_SLEEP;
		}
	}
	
	ASSERT(Vad->View);
	
	size_t ViewOffset = Va - Vad->Node.StartVa + Vad->ViewOffset;
	
	PFDbgPrint("MiNormalFault: Attempting to resolve fault at VA %p using MiResolveViewFault.", Va);
	
	// Try to resolve the page fault now.
	int PfnPermissions = 0;
	MMPFN Pfn = PFN_INVALID;
	Status = MiResolveViewFault(
		Vad->View,
		ViewOffset,
		IsInstructionFetchFault ? PAGE_EXECUTE : PAGE_READ,
		&Pfn,
		&PfnPermissions
	);
	
	if (Status == STATUS_MORE_PROCESSING_REQUIRED)
	{
		PMMVIEW View = ObReferenceObjectByPointer(Vad->View);
		
		// Don't need the locks any more.  We'll refault anyway.
		MmUnlockVadList(VadList);
		MmUnlockSpace(SpaceUnlockIpl, Va);
		
		PFDbgPrint("MiNormalFault: For VA %p, more processing is required.  Perform it.", Va);
		
		Status = MiPerformAdditionalProcessingForViewFault(View, ViewOffset);
		
		if (FAILED(Status))
		{
			DbgPrint(
				"MiNormalFault: MiPerformAdditionalProcessingForViewFault on VA %p failed: %s",
				Va,
				RtlGetStatusString(Status)
			);
			return Status;
		}
		
		return STATUS_REFAULT;
	}
	
	// Now assign the PTE.
	Status = MmpAssignPfnToAddress(Va, Pfn, PfnPermissions);
	if (FAILED(Status))
	{
		// Out of memory.
		DbgPrint(
			"MiNormalFault: Cannot map PFN into memory at VA %p: %s",
			Va,
			RtlGetStatusString(Status)
		);
		Status = STATUS_REFAULT_SLEEP;
		MmFreePhysicalPage(Pfn);
	}
	
	MmUnlockVadList(VadList);
	MmUnlockSpace(SpaceUnlockIpl, Va);
	*RefaultForWrite = false;
	return Status;
}

BSTATUS MiWriteFault(UNUSED PEPROCESS Process, uintptr_t Va, PMMPTE PtePtr)
{
	// This is a write fault:
	//
	// There are more than two types, but this is what we handle right now:
	// - Copy on write fault
	// - Access violation
	//
	// NOTE: IPL is raised to APC level and the relevant address space's lock is held.
	
	if (MmGetPageBitsPte(*PtePtr) & MM_PROT_WRITE)
	{
		// Spurious fault
		return STATUS_SUCCESS;
	}
	
	// We'll need the VAD to check the range's properties.
	// Now, MiNormalFault would have brought this PTE into existence, so we only really
	// need to check if the VAD allows CoW, or if the VAD allows direct writing.
	BSTATUS Status;
	PMMVAD_LIST VadList = MmpLockVadListByAddress(Process, Va);
	PMMVAD Vad = MmLookUpVadByAddress(VadList, Va);
	if (!Vad)
	{
		// There is no VAD at this address.
		MmUnlockVadList(VadList);
		
		// However, check if this is a demand-page pool address.
		uintptr_t PoolStart = MiGetTopOfPoolManagedArea();
		uintptr_t PoolEnd = PoolStart + (1ULL << MI_POOL_LOG2_SIZE);
		
	#ifdef MI_USE_TWO_POOLS
		uintptr_t Pool2Start = MiGetTopOfSecondPoolManagedArea();
		uintptr_t Pool2End = PoolStart + (1ULL << MI_POOL_LOG2_SIZE_2ND);
	#endif
		
		if ((PoolStart <= Va && Va < PoolEnd)
		#ifdef MI_USE_TWO_POOLS
			|| (Pool2Start <= Va && Va < Pool2End)
		#endif
			)
		{
			// Is in a pool area, make the existing PTE read-write
			PMMPTE PtePtr = MmGetPteLocationCheck(Va, true);
			*PtePtr = MmSetPageBitsPte(*PtePtr, MmGetPageBitsPte(*PtePtr) | MM_PROT_READ | MM_PROT_WRITE | MM_MISC_IS_FROM_PMM);
			return STATUS_SUCCESS;
		}
		
		// TODO: But there might be a system wide file map going on!
		DbgPrint("%s: Declaring access violation on VA %p. No VAD and not in a pool area. PoolStart:%p PoolEnd:%p", __func__, Va, PoolStart, PoolEnd);
		return STATUS_ACCESS_VIOLATION;
	}
	
	PFDbgPrint("MiWriteFault: Attempting to resolve fault at VA %p using MiResolveViewFault.", Va);
	
	MMPTE OldPte = *PtePtr;
	MMPFN OldPfn, NewPfn;
	uintptr_t PteFlags;
	int PfnPermissions;
	
	PteFlags = MmGetPageBitsPte(OldPte);
	OldPfn = MmGetPfnPte(OldPte);
	Status = MiResolveViewFault(
		Vad->View,
		Va - Vad->Node.StartVa + Vad->ViewOffset, // Address
		PAGE_WRITE, // Intent
		&NewPfn,
		&PfnPermissions
	);
	
	if (FAILED(Status))
	{
		DbgPrint("MiWriteFault: Cannot resolve write fault for VA %p: %s", Va, RtlGetStatusString(Status));
		MmUnlockVadList(VadList);
		return Status;
	}
	
	ASSERT(MmIsFromPmmPte(OldPte));
	ASSERT(PfnPermissions & PAGE_WRITE);
	
	// Fault resolved and we have a PFN to map now.
	*PtePtr = MmBuildPte(NewPfn, PteFlags | MM_PROT_WRITE);
	MmFlushTlbUpdates();

	// Free the old PFN.
	MmFreePhysicalPage(OldPfn);
	
	PFDbgPrint("MiWriteFault: VA %p upgraded to write successfully!", Va);
	MmUnlockVadList(VadList);
	return STATUS_SUCCESS;
}

// Returns: If the page fault failed to be handled, then the reason why.
BSTATUS MmPageFault(UNUSED uintptr_t FaultPC, uintptr_t FaultAddress, uintptr_t FaultMode)
{
	bool IsKernelSpace = false;
	bool IsUserModeCode = false;
	BSTATUS Status = STATUS_SUCCESS;
	PEPROCESS Process = PsGetAttachedProcess();
	
	if (FaultAddress >= MM_KERNEL_SPACE_BASE)
	{
		IsKernelSpace = true;
		Process = &PsSystemProcess;
	}
	
	if (FaultPC < MM_KERNEL_SPACE_BASE)
		IsUserModeCode = true;
	
	// TODO: Normally I'd use KeGetPreviousMode(). But it turns out we do take page faults
	// from kernel mode during system calls, *on* kernel space addresses, so make sure that
	// doesn't result in a crash.
	if (IsUserModeCode && IsKernelSpace)
	{
		// No, no, no.  You cannot access kernel mode data from user mode.
		return STATUS_ACCESS_VIOLATION;
	}
	
	// TODO: There may be conditions where accesses to kernel space are done on
	// behalf of a user process.
	
	// Attempt to interpret the PTE.
	KIPL OldIpl = MmLockSpaceExclusive(FaultAddress);
	
	PMMPTE PtePtr = MmGetPteLocationCheck(FaultAddress, false);
	
	// If the PTE is present.
	if (PtePtr && MmIsPresentPte(*PtePtr))
	{
		// If this is a user mode thread and it's trying to access kernel mode addresses,
		// declare failure instantly.
		if ((~MmGetPageBitsPte(*PtePtr) & MM_PROT_USER) && IsUserModeCode)
		{
			PFDbgPrint("AV: Page is valid, but this is a user mode access for a kernel region.");
			Status = STATUS_ACCESS_VIOLATION;
			MmUnlockSpace(OldIpl, FaultAddress);
			goto EarlyExit;
		}
		
		if ((~MmGetPageBitsPte(*PtePtr) & MM_PROT_EXEC) && (FaultMode & MM_FAULT_INSNFETCH))
		{
			PFDbgPrint("AV: Page is valid, but this is an attempt to execute code that isn't marked executable.");
			Status = STATUS_ACCESS_VIOLATION;
			MmUnlockSpace(OldIpl, FaultAddress);
			goto EarlyExit;
		}
		
		if (FaultMode & MM_FAULT_WRITE)
		{
			// A write fault occurred on a readonly page.
			Status = MiWriteFault(Process, FaultAddress, PtePtr);
			MmUnlockSpace(OldIpl, FaultAddress);
			goto EarlyExit;
		}
		
		// The PTE is valid, and this wasn't a write fault, so this fault may have been
		// spurious and we should simply return.
		Status = STATUS_SUCCESS;
		MmUnlockSpace(OldIpl, FaultAddress);
		return Status;
	}
	
	// The PTE is not present.
	//
	// Note: MiNormalFault will unlock the memory space.
	bool RefaultForWrite = true;
	Status = MiNormalFault(Process, FaultAddress, PtePtr, OldIpl, (FaultMode & MM_FAULT_INSNFETCH), &RefaultForWrite);
	
	if (SUCCEEDED(Status) && (FaultMode & MM_FAULT_WRITE) && RefaultForWrite)
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
		DbgPrint("MmPageFault: Out of memory, sleeping %d ms waiting for more memory.", MI_REFAULT_SLEEP_MS);
		
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
		
		Status = KeWaitForSingleObject(&Timer, true, TIMEOUT_INFINITE, KeGetPreviousMode());
		if (FAILED(Status))
		{
			DbgPrint("MmPageFault: Failed to sleep?! %s (%d)", RtlGetStatusString(Status), Status);
			KeCancelTimer(&Timer);
		}
		else
		{
			Status = STATUS_REFAULT;
		}
	}
	
	return Status;
}
