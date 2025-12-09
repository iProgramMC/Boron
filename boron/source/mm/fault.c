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

static BSTATUS MmpHandleFaultCommittedPage(PMMPTE PtePtr, MMPTE SupervisorBit)
{
	// This PTE is demand paged.  Allocate a page.
	MMPFN Pfn = MmAllocatePhysicalPage();
	
	if (Pfn == PFN_INVALID)
	{
		// TODO: This is probably bad
		return STATUS_REFAULT_SLEEP;
	}
	
	// Create a new, valid, PTE that will replace the current one.
	MMPTE NewPte = *PtePtr;
	NewPte &= ~MM_DPTE_COMMITTED;
	NewPte |=  MM_PTE_PRESENT | MM_PTE_ISFROMPMM | SupervisorBit;
	NewPte |=  MmPFNToPhysPage(Pfn);
	NewPte &= ~MM_PTE_PKMASK;
	*PtePtr = NewPte;
	
	// Fault was handled successfully.
	return STATUS_SUCCESS;
}

static BSTATUS MmpAssignPfnToAddress(uintptr_t Va, MMPFN Pfn)
{
	MMPTE SupervisorBit = Va >= MM_KERNEL_SPACE_BASE ? 0 : MM_PTE_USERACCESS;
	
	PMMPTE PtePtr = MmGetPteLocationCheck(Va, true);
	if (!PtePtr)
		return STATUS_INSUFFICIENT_MEMORY;
	
	MMPTE NewPte = MM_PTE_PRESENT | MM_PTE_ISFROMPMM | SupervisorBit | MmPFNToPhysPage(Pfn);
	
	*PtePtr = NewPte;
	
	return STATUS_SUCCESS;
}

static BSTATUS MmpHandleFaultCommittedMappedPage(
	uintptr_t Va,
	uintptr_t VaBase,
	uint64_t MappedOffset,
	void* MappedObject,
	KIPL SpaceUnlockIpl
)
{
	// NOTE: IPL is raised to APC level and the relevant address
	// space's lock is held.  The VAD lock is not held.
	//
	// The address space's lock **must be released** at the end of
	// this function.
	//
	// TODO: Perhaps we should implement a per-VAD mutex?  I'm not
	// sure that's required, personally.  The case this would prevent
	// is that the VAD is unmapped by the time the I/O operation
	// that we will be doing finishes.  Then, the page is mapped into
	// memory.
	//
	// This would end up with a page from the wrong VAD that was
	// released, and potentially another VAD could be reserved
	// in this very spot.
	//
	// Now, this is obviously just user error, but can this be exploited?
	// Probably not.  When the page fault finishes, 
	BSTATUS Status;
	
	const uintptr_t PageMask = ~(PAGE_SIZE - 1);
	uint64_t SectionOffset = ((Va & PageMask) - VaBase + MappedOffset) / PAGE_SIZE;
	
	MMPFN Pfn = PFN_INVALID;
	
	// First, check if the page even exists.
	PFDbgPrint("%s: For VA %p, calling MmGetPageMappable", __func__, Va);
	Status = MmGetPageMappable(MappedObject, SectionOffset, &Pfn);
	
	if (Status == STATUS_MORE_PROCESSING_REQUIRED)
	{
		// It doesn't exist, so we need to fetch it manually.
		MmUnlockSpace(SpaceUnlockIpl, Va);
		SpaceUnlockIpl = -1;
		
		PFDbgPrint("%s: For VA %p, calling MmReadPageMappable", __func__, Va);
		Status = MmReadPageMappable(MappedObject, SectionOffset, &Pfn);
		if (FAILED(Status))
		{
			DbgPrint("%s: MmReadPageMappable failed to fulfill request with code %d", __func__, Status);
			goto Exit;
		}
		
		SpaceUnlockIpl = MmLockSpaceExclusive(Va);
	}
	
	ASSERT(Pfn != PFN_INVALID);
	
	PFDbgPrint("%s: For VA %p, calling MmpAssignPfnToAddress(%d)", __func__, Va, Pfn);
	Status = MmpAssignPfnToAddress(Va, Pfn);
	if (FAILED(Status))
	{
		MmFreePhysicalPage(Pfn);
		
		DbgPrint("%s: out of memory because PTE couldn't be allocated (1)", __func__);
		ASSERT(Status == STATUS_INSUFFICIENT_MEMORY);
		Status = STATUS_REFAULT_SLEEP;
		goto Exit;
	}
	
	PFDbgPrint("%s: hooray! page fault fulfilled by cached fetch %p", __func__, Va);
	
Exit:
	ObDereferenceObject(MappedObject);
	
	if (SpaceUnlockIpl >= 0)
		MmUnlockSpace(SpaceUnlockIpl, Va);
	
	if (FAILED(Status))
		DbgPrint("%s: failed to fulfill page fault with code %d", __func__, Status);
	
	return Status;
}

BSTATUS MiNormalFault(PEPROCESS Process, uintptr_t Va, PMMPTE PtePtr, KIPL SpaceUnlockIpl)
{
	// NOTE: IPL is raised to APC level and the relevant address space's lock is held.
	bool IsPageCommitted = false;
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
		
	#ifdef TARGET_I386
		uintptr_t Pool2Start = MiGetTopOfSecondPoolManagedArea();
		uintptr_t Pool2End = PoolStart + (1ULL << MI_POOL_LOG2_SIZE_2ND);
	#endif
		
		if ((PoolStart <= Va && Va < PoolEnd)
		#ifdef TARGET_I386
			|| (Pool2Start <= Va && Va < Pool2End)
		#endif
			)
		{
			// Is in a pool area, so allocate a page of memory and map it there.
			PMMPTE PtePtr = MmGetPteLocationCheck(Va, true);
			if (*PtePtr & MM_DPTE_COMMITTED)
				return MmpHandleFaultCommittedPage(PtePtr, 0);
		}
		
		DbgPrint("MiNormalFault: Declaring access violation on VA %p. No VAD and not in a pool area", Va);
		return STATUS_ACCESS_VIOLATION;
	}
	
	// If there is no PTE or it's zero.
	if (!PtePtr || !*PtePtr)
	{
		if (!Vad->Flags.Committed)
		{
			// The PTE is NULL yet the VAD isn't fully committed (they didn't request
			// memory with MEM_RESERVE | MEM_COMMIT). Therefore, access violation it is.
			MmUnlockVadList(VadList);
			MmUnlockSpace(SpaceUnlockIpl, Va);
			DbgPrint("MiNormalFault: Declaring access violation on VA %p because there is an uncommitted VAD and the region was not committed separately.", Va);
			return STATUS_ACCESS_VIOLATION;
		}
		
		// VAD is committed, so this page fault can be resolved.  But first, try to allocate
		// the PTE itself.
		if (!PtePtr)
		{
			PtePtr = MmGetPteLocationCheck(Va, true);
			
			// If the PTE couldn't be allocated, return with STATUS_REFAULT_SLEEP, waiting for more
			// memory.
			if (!PtePtr)
			{
				MmUnlockVadList(VadList);
				MmUnlockSpace(SpaceUnlockIpl, Va);
				return STATUS_REFAULT_SLEEP;
			}
		}
		
		IsPageCommitted = true;
	}
	
	// There is a PTE pointer, check if it's committed.
	if (!IsPageCommitted && PtePtr && (*PtePtr & MM_DPTE_COMMITTED))
	{
		IsPageCommitted = true;
	}
	
	if (!IsPageCommitted)
	{
		DbgPrint("MiNormalFault: invalid page fault at address %p!", Va);
		MmUnlockSpace(SpaceUnlockIpl, Va);
		return STATUS_ACCESS_VIOLATION;
	}
	
	// Check if there is an object to read from.
	if (Vad->MappedObject)
	{
		void* Object = ObReferenceObjectByPointer(Vad->MappedObject);
		uintptr_t VaBase = Vad->Node.StartVa;
		uint64_t VadMappedOffset = Vad->SectionOffset;
		
		// (Access to the VAD is no longer required now)
		MmUnlockVadList(VadList);
		
		// There is.  Handle this page fault separately.
		return MmpHandleFaultCommittedMappedPage(Va, VaBase, VadMappedOffset, Object, SpaceUnlockIpl);
	}
	
	// Now the PTE is here and we can commit it.
	*PtePtr = Vad->Flags.Protection;
	
	// (Access to the VAD is no longer required now)
	MmUnlockVadList(VadList);
	
	bool IsViewSpace = Va >= MM_KERNEL_SPACE_BASE;
	Status = MmpHandleFaultCommittedPage(PtePtr, IsViewSpace ? 0 : MM_PTE_USERACCESS);

	// This is all we needed to do for the non-object-backed case.
	MmUnlockSpace(SpaceUnlockIpl, Va);
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
	
	if (*PtePtr & MM_PTE_READWRITE)
	{
		// Spurious fault
		return STATUS_SUCCESS;
	}
	
	// We'll need the VAD to check the range's properties.
	// Now, MiNormalFault would have brought this PTE into existence, so we only really
	// need to check if the VAD allows CoW, or if the VAD allows direct writing.
	PMMVAD_LIST VadList = MmpLockVadListByAddress(Process, Va);
	PMMVAD Vad = MmLookUpVadByAddress(VadList, Va);
	if (!Vad)
	{
		// There is no VAD at this address.
		MmUnlockVadList(VadList);
		
		// However, check if this is a demand-page pool address.
		uintptr_t PoolStart = MiGetTopOfPoolManagedArea();
		uintptr_t PoolEnd = PoolStart + (1ULL << MI_POOL_LOG2_SIZE);
		
	#ifdef TARGET_I386
		uintptr_t Pool2Start = MiGetTopOfSecondPoolManagedArea();
		uintptr_t Pool2End = PoolStart + (1ULL << MI_POOL_LOG2_SIZE_2ND);
	#endif
		
		if ((PoolStart <= Va && Va < PoolEnd)
		#ifdef TARGET_I386
			|| (Pool2Start <= Va && Va < Pool2End)
		#endif
			)
		{
			// Is in a pool area, make the existing PTE read-write
			PMMPTE PtePtr = MmGetPteLocationCheck(Va, true);
			*PtePtr |= MM_PTE_READWRITE;
			return STATUS_SUCCESS;
		}
		
		// TODO: But there might be a system wide file map going on!
		DbgPrint("%s: Declaring access violation on VA %p. No VAD and not in a pool area. PoolStart:%p PoolEnd:%p", __func__, Va, PoolStart, PoolEnd);
		return STATUS_ACCESS_VIOLATION;
	}
	
	if (~Vad->Flags.Protection & PAGE_WRITE)
	{
		DbgPrint("%s: Declaring access violation on VA %p because this page isn't mapped writable. PTE: %p", __func__, Va, *PtePtr);
		MmUnlockVadList(VadList);
		return STATUS_ACCESS_VIOLATION;
	}
	
	if (Vad->MappedObject != NULL)
	{
		// Write allowed, so tell the mapped object that it should prepare for writes.
		const uintptr_t PageMask = ~(PAGE_SIZE - 1);
		uint64_t SectionOffset = ((Va & PageMask) - Vad->Node.StartVa + Vad->SectionOffset) / PAGE_SIZE;
		
		PFDbgPrint("%s: For VA %p, calling MmPrepareWriteMappable.", __func__, Va);
		BSTATUS Status = MmPrepareWriteMappable(Vad->MappedObject, SectionOffset);
		if (FAILED(Status))
		{
			DbgPrint(
				"%s: Cannot make VA %p writable because MmPrepareWriteMappable returned status %d. %s",
				__func__,
				Va,
				Status,
				RtlGetStatusString(Status)
			);
			MmUnlockVadList(VadList);
			return Status;
		}
		
		PFDbgPrint("%s: For VA %p, calling MmGetPageMappable.", __func__, Va);
		MMPFN NewPfn = PFN_INVALID;
		Status = MmGetPageMappable(Vad->MappedObject, SectionOffset, &NewPfn);
		
		if (FAILED(Status))
		{
			// NOTE: A normal fault should've been handled first, to bring the page's data
			// in from the backing store.
			DbgPrint(
				"%s: Cannot make VA %p writable because MmGetPageMappable returned status %d. %s",
				__func__,
				Va,
				Status,
				RtlGetStatusString(Status)
			);
			MmUnlockVadList(VadList);
			return Status;
		}
		
		PFDbgPrint("%s: For VA %p, using PFN %d.", __func__, Va, NewPfn);
		*PtePtr =
			(*PtePtr & ~(MM_PTE_COW | MM_PTE_ADDRESSMASK)) |
			MM_PTE_READWRITE |
			(NewPfn * PAGE_SIZE);
	}
	else
	{
		// Anonymous memory, so just upgrade permissions
		PFDbgPrint("%s: Upgrading permissions because this is anonymous memory.", __func__);
		*PtePtr |= MM_PTE_READWRITE;
	}
	
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
	if (PtePtr && (*PtePtr & MM_PTE_PRESENT))
	{
		// If this is a user mode thread and it's trying to access kernel mode addresses,
		// declare failure instantly.
		if ((~*PtePtr & MM_PTE_USERACCESS) && IsUserModeCode)
		{
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
	Status = MiNormalFault(Process, FaultAddress, PtePtr, OldIpl);
	
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
