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

static BSTATUS MmpHandleFaultCommittedPage(PMMPTE PtePtr, MMPTE SupervisorBit)
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
	NewPte |=  MM_PTE_PRESENT | MM_PTE_ISFROMPMM | SupervisorBit;
	NewPte |=  MmPFNToPhysPage(Pfn);
	NewPte &= ~MM_PTE_PKMASK;
	*PtePtr = NewPte;
	
	// Fault was handled successfully.
	return STATUS_SUCCESS;
}

static BSTATUS MmpHandleFaultCommittedMappedPage(
	uintptr_t Va,
	uintptr_t VaBase,
	uint32_t VadFlagsLong,
	uint64_t MappedOffset,
	void* Object,
	bool IsFile,
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
	PMMPTE PtePtr = NULL;
	
	MMPTE SupervisorBit = Va >= MM_KERNEL_SPACE_BASE ? 0 : MM_PTE_USERACCESS;
	
	MMVAD_FLAGS VadFlags;
	VadFlags.LongFlags = VadFlagsLong;
	
	const uintptr_t PageMask = ~(PAGE_SIZE - 1);
	uint64_t FileOffset = (Va & PageMask) - VaBase + MappedOffset;
	
	// Is this a section?
	if (!IsFile)
	{
		// TODO: Implement sections.  Note, they could probably
		// share a lot of code with the file I/O cache stuff.
		Status = STATUS_UNIMPLEMENTED;
	}
	else
	{
		PFILE_OBJECT FileObject = (PFILE_OBJECT) Object;
		PCCB CacheBlock = &FileObject->Fcb->CacheBlock;
		
		MMPFN Pfn = PFN_INVALID;
		
		MmLockCcb(CacheBlock);
		
		PCCB_ENTRY PCcbEntry = MmGetEntryPointerCcb(CacheBlock, FileOffset / PAGE_SIZE, false);
		if (PCcbEntry && AtLoad(PCcbEntry->Long))
		{
			// Lock the physical memory lock because the page we're looking for
			// might have been freed.
			//
			// This is, as far as I know, the only way.  Sure, if the CCB entry
			// doesn't exist and it is zero, then you are able to skip over the
			// PFN lock at no extra cost (you'd just be reloading the same page
			// twice potentially)
			KIPL Ipl = MiLockPfdb();
			
			// After this, the CCB entry cannot be changed.  This is because
			// when a page is reclaimed the PFDB lock is held.
			CCB_ENTRY CcbEntry;
			if (PCcbEntry)
				CcbEntry = *PCcbEntry;
			
			if (CcbEntry.Long)
			{
				// We managed to find the PFN in one piece.  Add a reference to it as we
				// will just map it directly.
				Pfn = CcbEntry.U.Pfn;
				MiPageAddReferenceWithPfdbLocked(Pfn);
			}
			
			MiUnlockPfdb(Ipl);
		}
		
		MmUnlockCcb(CacheBlock);
		
		// Did we find the PFN already?
		if (Pfn != PFN_INVALID)
		{
			// As it turns out, yes!
			PtePtr = MmGetPteLocationCheck(Va, true);
			if (!PtePtr)
			{
				DbgPrint("%s: out of memory because PTE couldn't be allocated (1)", __func__);
				Status = STATUS_REFAULT_SLEEP;
				goto Exit;
			}
			
			MMPTE NewPte = MM_PTE_PRESENT | MM_PTE_ISFROMPMM | SupervisorBit | MmPFNToPhysPage(Pfn);
			
			if (VadFlags.Cow)
				NewPte |= MM_PTE_COW;
			
			*PtePtr = NewPte;
			
			MmSetPrototypePtePfn(Pfn, &PCcbEntry->Long);
			
			DbgPrint("%s: hooray! page fault fulfilled by cached fetch %p", __func__, PtePtr);
			Status = STATUS_SUCCESS;
			goto Exit;
		}
		
		// No, we didn't find the PFN already.
		MmUnlockSpace(SpaceUnlockIpl, Va);
		SpaceUnlockIpl = -1;
		
		// First, ensure the CCB entry can be there.
		MmLockCcb(CacheBlock);
		PCcbEntry = MmGetEntryPointerCcb(CacheBlock, FileOffset / PAGE_SIZE, true);
		MmUnlockCcb(CacheBlock);
		
		if (!PCcbEntry)
		{
			// Out of memory.
			DbgPrint("%s: out of memory because CCB entry couldn't be allocated (1)", __func__);
			Status = STATUS_REFAULT_SLEEP;
			goto Exit;
		}
		
		// Allocate a PFN now, do a read, and then put it into the CCB.
		Pfn = MmAllocatePhysicalPage();
		if (Pfn == PFN_INVALID)
		{
			// Out of memory.
			DbgPrint("%s: out of memory because a page frame couldn't be allocated", __func__);
			MmUnlockCcb(CacheBlock);
			Status = STATUS_REFAULT_SLEEP;
			goto Exit;
		}
		
		// Okay.  Let's perform the IO on this PFN.
		MDL_ONEPAGE Mdl;
		MmInitializeSinglePageMdl(&Mdl, Pfn, MDL_FLAG_WRITE);
		
		IO_STATUS_BLOCK Iosb;
		Status = IoPerformPagingRead(
			&Iosb,
			FileObject,
			&Mdl.Base,
			FileOffset
		);
		
		MmFreeMdl(&Mdl.Base);
		
		if (FAILED(Status))
		{
			// Ran out of memory trying to read this file.
			// Let the MM know we need more memory.
			DbgPrint("%s: cannot answer page fault because I/O failed with code %d", __func__, Status);
			MmFreePhysicalPage(Pfn);
			MmUnlockCcb(CacheBlock);
			if (Status == STATUS_INSUFFICIENT_MEMORY)
				Status = STATUS_REFAULT_SLEEP;
			goto Exit;
		}
		
		// The I/O operation has succeeded.
		// Prepare the CCB entry.
		CCB_ENTRY CcbEntryNew;
		CcbEntryNew.Long = 0;
		CcbEntryNew.U.Pfn = Pfn;
		
		// It's time to put this in the CCB and then map.
		MmLockCcb(CacheBlock);
		PCcbEntry = MmGetEntryPointerCcb(CacheBlock, FileOffset / PAGE_SIZE, true);
		if (!PCcbEntry)
		{
			// Out of memory.  Did someone remove our prior allocation?
			// Well, we've got almost no choice but to throw away our result.
			//
			// (We could instead beg the memory manager for more memory, but
			// I don't feel like writing that right now.)
			DbgPrint("%s: out of memory because CCB entry couldn't be allocated (2)", __func__);
			MmFreePhysicalPage(Pfn);
			MmUnlockCcb(CacheBlock);
			Status = STATUS_REFAULT_SLEEP;
			goto Exit;
		}
		
		// There you are!
		//
		// If the CCB entry is zero, then it'll be populated with the
		// new CCB entry.  Otherwise, there's something already there
		// and we have simply wasted our time and we need to refault.
		uint64_t Expected = 0;
		if (AtCompareExchange(&PCcbEntry->Long, &Expected, CcbEntryNew.Long))
		{
			// Compare-exchange successful.  Note: we are surrendering
			// our reference of the physical page to the CCB.
			MmUnlockCcb(CacheBlock);
			
			// Acquire the address space lock now.
			SpaceUnlockIpl = MmLockSpaceExclusive(Va);
			
			// We need to refetch the PTE pointer because it may have been
			// freed in the meantime.
			PtePtr = MmGetPteLocationCheck(Va, true);
			if (!PtePtr)
			{
				DbgPrint("%s: out of memory because PTE couldn't be allocated (2)", __func__);
				MmFreePhysicalPage(Pfn);
				MmUnlockCcb(CacheBlock);
				Status = STATUS_REFAULT_SLEEP;
				goto Exit;
			}
			
			MMPTE NewPte = *PtePtr;
			
			// Are you already present?!
			if (NewPte & MM_PTE_PRESENT)
			{
				// Yeah, you seem to already be present.  So we just
				// wasted all of our time.
				DbgPrint("%s: va already valid by the time IO was made (%p)", __func__, Va);
				MmFreePhysicalPage(Pfn);
				Status = STATUS_SUCCESS;
				goto Exit;
			}
			
			NewPte = MM_PTE_PRESENT | MM_PTE_ISFROMPMM | SupervisorBit | MmPFNToPhysPage(Pfn);
			
			if (VadFlags.Cow)
				NewPte |= MM_PTE_COW;
			
			MmSetPrototypePtePfn(Pfn, &PCcbEntry->Long);
			
			*PtePtr = NewPte;
			
			DbgPrint("%s: hooray! page fault fulfilled by I/O read", __func__);
			Status = STATUS_SUCCESS;
			goto Exit;
		}
		
		// Something's already there!
		// It's most likely a good page, so let's just refault.
		DbgPrint("%s: ccb already found by the time IO was made (%p), refaulting", __func__, Va);
		Status = STATUS_REFAULT;
		MmUnlockCcb(CacheBlock);
	}
	
Exit:
	ObDereferenceObject(Object);
	
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
	PMMVAD_LIST VadList = MmLockVadListProcess(Process);
	PMMVAD Vad = MmLookUpVadByAddress(VadList, Va);
	
	if (!Vad)
	{
		// There is no VAD at this address.
		
		// TODO: But there might be a system wide file map going on!
		MmUnlockVadList(VadList);
		MmUnlockSpace(SpaceUnlockIpl, Va);
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
		DbgPrint("MiNormalFault: page fault at address %p!", Va);
		MmUnlockSpace(SpaceUnlockIpl, Va);
		return STATUS_ACCESS_VIOLATION;
	}
	
	// Check if there is an object to read from.
	if (Vad->Mapped.Object)
	{
		void* Object = ObReferenceObjectByPointer(Vad->Mapped.Object);
		bool IsFile = Vad->Flags.IsFile;
		uintptr_t VaBase = Vad->Node.StartVa;
		uint32_t VadFlagsLong = Vad->Flags.LongFlags;
		uint64_t VadMappedOffset = Vad->SectionOffset;
		
		// (Access to the VAD is no longer required now)
		MmUnlockVadList(VadList);
		
		// There is.  Handle this page fault separately.
		return MmpHandleFaultCommittedMappedPage(Va, VaBase, VadFlagsLong, VadMappedOffset, Object, IsFile, SpaceUnlockIpl);
	}
	
	// Now the PTE is here and we can commit it.
	*PtePtr = Vad->Flags.Protection;
	
	// (Access to the VAD is no longer required now)
	MmUnlockVadList(VadList);
	
	Status = MmpHandleFaultCommittedPage(PtePtr, Va >= MM_KERNEL_SPACE_BASE ? 0 : MM_PTE_USERACCESS);

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
	
	if (*PtePtr & MM_PTE_COW)
	{
		// Lock the PFDB to check the reference count of the page.
		//
		// The page, normally, would only be copied if the page has a reference count of more than one.
		// However, the page may actually be part of a page cache which the caller definitely didn't
		// want to overwrite.
		KIPL Ipl = MiLockPfdb();
		
		int RefCount = 1;
		MMPFN Pfn = PFN_INVALID;
		
		if (*PtePtr & MM_PTE_ISFROMPMM)
		{
			Pfn = (*PtePtr & MM_PTE_ADDRESSMASK) / PAGE_SIZE;
			RefCount = MiGetReferenceCountPfn(Pfn);
			ASSERT(RefCount > 0);
		}
		
		if (false && RefCount == 1)
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
			if (Pfn != PFN_INVALID)
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
	PEPROCESS Process = PsGetAttachedProcess();
	
	if (FaultAddress >= MM_KERNEL_SPACE_BASE)
	{
		IsKernelSpace = true;
		Process = &PsSystemProcess;
	}
	
	// TODO: There may be conditions where accesses to kernel space are done on
	// behalf of a user process.
	
	// Attempt to interpret the PTE.
	KIPL OldIpl = MmLockSpaceExclusive(FaultAddress);
	
	PMMPTE PtePtr = MmGetPteLocationCheck(FaultAddress, false);
	
	// If the PTE is present.
	if (PtePtr && (*PtePtr & MM_PTE_PRESENT))
	{
		// The PTE exists and is present.
		if (*PtePtr & MM_PTE_READWRITE)
		{
			// The PTE is writable already, therefore this fault may have been spurious
			// and we should simply return.
			DbgPrint("Returning bc the PTE is writable");
			Status = STATUS_SUCCESS;
			MmUnlockSpace(OldIpl, FaultAddress);
			return Status;
		}
		
		if (FaultMode & MM_FAULT_WRITE)
		{
			// A write fault occurred on a readonly page.
			DbgPrint("Handling a write fault");
			Status = MiWriteFault(Process, FaultAddress, PtePtr);
			MmUnlockSpace(OldIpl, FaultAddress);
			goto EarlyExit;
		}
		
		// The PTE is valid, and this wasn't a write fault, so this fault may have been
		// spurious and we should simply return.
		DbgPrint("Returning because spurious fault  %p  %p  %p", FaultAddress, PtePtr, *PtePtr);
		Status = STATUS_SUCCESS;
		MmUnlockSpace(OldIpl, FaultAddress);
		return Status;
	}
	
	// The PTE is not present.
	//
	// Note: MiNormalFault will unlock the memory space.
	DbgPrint("Handling a normal fault");
	Status = MiNormalFault(Process, FaultAddress, PtePtr, OldIpl);
	
	if (SUCCEEDED(Status) && (FaultMode & MM_FAULT_WRITE))
	{
		// The PTE was made valid, but it might still be readonly.  Refault
		// to make the page writable.  Note that returning STATUS_REFAULT
		// simply brings us back into MmPageFault instead of going all the
		// way back to the offending process to save some cycles.
		DbgPrint("Refault because write");
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
		DbgPrint("Refault sleep because out of memory");
		
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, MI_REFAULT_SLEEP_MS, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE, MODE_KERNEL);
		
		DbgPrint("Out of memory!");
		
		Status = STATUS_REFAULT;
	}
	
	return Status;
}
