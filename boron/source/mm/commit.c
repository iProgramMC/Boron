/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/commit.c
	
Abstract:
	This module defines functions related to virtual memory
	commit/decommit.
	
Author:
	iProgramInCpp - 9 March 2025
***/
#include <mm.h>
#include <ex.h>

//
// Commits a range of virtual memory, with anonymous pages.
//
// The entire memory region must be uncommitted.
//
BSTATUS MmCommitVirtualMemory(uintptr_t StartVa, size_t SizePages, int Protection)
{
	// Check if the provided address range is valid.
	if (!MmIsAddressRangeValid(StartVa, SizePages * PAGE_SIZE, MODE_USER))
		return STATUS_INVALID_PARAMETER;
	
	// Check if the range passed in overlaps two or more VADs, or none
	PMMVAD_LIST VadList = MmLockVadList();
	
	PMMVAD Vad = MmLookUpVadByAddress(VadList, StartVa);
	PMMVAD VadEnd = MmLookUpVadByAddress(VadList, StartVa + SizePages * PAGE_SIZE - 1);
	
	// If the ranges do not match, or if any of them is NULL,
	// then a commit operation cannot happen.
	if (Vad != VadEnd || Vad == NULL)
	{
		MmUnlockVadList(VadList);
		DbgPrint("VAD %p doesn't match VAD end %p", Vad, VadEnd);
		return STATUS_CONFLICTING_ADDRESSES;
	}
	
	bool IsVadCommitted = Vad->Flags.Committed;
	
	if (!Protection)
		Protection = Vad->Flags.Protection;
	
#ifdef DEBUG
	Vad = NULL;
	VadEnd = NULL;
#endif
	
	MmUnlockVadList(VadList);
	
	if (!Protection)
		return STATUS_INVALID_PARAMETER;
	
	// If this range is committed, check if the PTEs are marked decommitted.
	// Otherwise, check if they are null.
	BSTATUS Status = STATUS_SUCCESS;
	KIPL Ipl = MmLockSpaceExclusive(StartVa);
	
	PMMPTE Pte = MmGetPteLocation(StartVa);
	uintptr_t CurrentVa = StartVa;
	for (size_t i = 0; i < SizePages; )
	{
		// If the Pte address crossed into a new page, or if the range is equal to zero,
		// we need to check if that new page is valid.
		if (i == 0 || ((uintptr_t)Pte & 0xFFF) == 0)
		{
			if (!MmCheckPteLocation(CurrentVa, false))
			{
				// There is no PTE page.  If the VAD has the committed flag active,
				// this means all the PTEs on this page are committed, therefore break.
				if (IsVadCommitted)
				{
					DbgPrint("VAD committed but PTE doesn't exist");
					Status = STATUS_CONFLICTING_ADDRESSES;
					break;
				}
				
				// Otherwise, skip this page of PTEs entirely and try again.
				size_t OldVa = CurrentVa;
				CurrentVa = (CurrentVa + PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE)) & ~(PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE) - 1);
				Pte = MmGetPteLocation(CurrentVa);
				i += (CurrentVa - OldVa) / PAGE_SIZE;
				continue;
			}
		}
		
		// The PTE is accessible.
		
		// If the PTE is present, there is overlap with an already committed region.
		// Ditto if the PTE is marked committed.
		if (*Pte & (MM_PTE_PRESENT | MM_DPTE_COMMITTED))
		{
			DbgPrint("PTE is present or marked committed");
			Status = STATUS_CONFLICTING_ADDRESSES;
			break;
		}
		
		// If the VAD is marked committed and the page we're looking at hasn't been
		// decommitted, there is overlap.
		if (IsVadCommitted && (~*Pte & MM_DPTE_DECOMMITTED))
		{
			DbgPrint("VAD is committed but PTE is not decommitted");
			Status = STATUS_CONFLICTING_ADDRESSES;
			break;
		}
		
		// No overlap, move right along.
		i++;
		Pte++;
		CurrentVa += PAGE_SIZE;
		continue;
	}
	
	if (FAILED(Status))
	{
		MmUnlockSpace(Ipl, StartVa);
		return Status;
	}
	
	// The range is safe to commit.
	uintptr_t PteFlags = MmGetPteBitsFromProtection(Protection);
	
	// TODO: Enforce W^X here if the user doesn't have the relevant permissions
	
	Pte = MmGetPteLocation(StartVa);
	CurrentVa = StartVa;
	for (size_t i = 0; i < SizePages; i++)
	{
		// If the Pte address crossed into a new page, or if the range is equal to zero,
		// we need to check if that new page is valid.
		if (i == 0 || ((uintptr_t)Pte & 0xFFF) == 0)
		{
			if (!MmCheckPteLocation(CurrentVa, true))
			{
				// Uh oh! We ran out of memory!
				// TODO: Roll back our changes, maybe?
				MmUnlockSpace(Ipl, StartVa);
				DbgPrint("Ran out of pages for PTE allocation");
				return STATUS_INSUFFICIENT_MEMORY;
			}
		}
		
		*Pte = MM_DPTE_COMMITTED | PteFlags;
		Pte++;
		CurrentVa += PAGE_SIZE;
	}
	
	// Okay, everything's committed. Success!
	MmUnlockSpace(Ipl, StartVa);
	return STATUS_SUCCESS;
}

void MiDecommitVad(PMMVAD_LIST VadList, PMMVAD Vad, size_t StartVa, size_t SizePages);

// Decommits a range of virtual memory.
BSTATUS MmDecommitVirtualMemory(uintptr_t StartVa, size_t SizePages)
{
	// Check if the provided address range is valid.
	if (!MmIsAddressRangeValid(StartVa, SizePages * PAGE_SIZE, MODE_USER) || SizePages == 0)
		return STATUS_INVALID_PARAMETER;
	
	// Check if the range passed in overlaps two or more VADs, or none
	PMMVAD_LIST VadList = MmLockVadList();
	
	PMMVAD Vad = MmLookUpVadByAddress(VadList, StartVa);
	PMMVAD VadEnd = MmLookUpVadByAddress(VadList, StartVa + SizePages * PAGE_SIZE - 1);
	
	// If the ranges do not match, or if any of them is NULL,
	// then a commit operation cannot happen.
	if (Vad != VadEnd || Vad == NULL)
	{
		MmUnlockVadList(VadList);
		DbgPrint("VAD %p doesn't match VAD end %p", Vad, VadEnd);
		return STATUS_CONFLICTING_ADDRESSES;
	}
	
	MiDecommitVad(VadList, Vad, StartVa, SizePages);
	return STATUS_SUCCESS;
}

// This part of MmDecommitVirtualMemory has been split into a separate function because
// this is also referenced by MmTearDownVadList().
void MiDecommitVad(PMMVAD_LIST VadList, PMMVAD Vad, size_t StartVa, size_t SizePages)
{
	// Check if the range can be entirely decommitted.
	if (Vad->Node.StartVa == StartVa && Vad->Node.Size == SizePages)
	{
		// Yes, just mark it as uncommitted.
		Vad->Flags.Committed = false;
	}
	
	bool IsVadCommitted = Vad->Flags.Committed;
	
#ifdef DEBUG
	Vad = NULL;
#endif
	
	MmUnlockVadList(VadList);
	
	// For this function, it doesn't actually matter if the region is partly decommitted.
	
	// Acquire the address space lock.  This prevents the address space from being mutated.
	KIPL Ipl = MmLockSpaceExclusive(StartVa);
	
	uintptr_t CurrentVa = StartVa;
	PMMPTE Pte = MmGetPteLocation(CurrentVa);
	for (size_t i = 0; i < SizePages; )
	{
		if (i == 0 || ((uintptr_t)Pte & 0xFFF) == 0)
		{
			// If IsVadCommitted is true, then we would need allocate new PTs
			// to mark the page as decommitted.  I am slightly embarrassed about this,
			// but it turns out that with some WinDbg magic I realized that Windows XP
			// does this exact same thing. So it'll be fine here too.
			if (!MmCheckPteLocation(CurrentVa, IsVadCommitted))
			{
				// If the VAD isn't committed, then nothing to decommit, skip this page of
				// PTEs entirely and try again.
				size_t OldVa = CurrentVa;
				CurrentVa = (CurrentVa + PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE)) & ~(PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE) - 1);
				Pte = MmGetPteLocation(CurrentVa);
				i += (CurrentVa - OldVa) / PAGE_SIZE;
				
			#ifdef DEBUG
				if (IsVadCommitted)
					DbgPrint("Warning: VAD is committed but can't allocate a PT to mark pages as decommitted... :(");
			#endif
				
				continue;
			}
		}
		
		if (*Pte & MM_PTE_PRESENT)
		{
			// The PTE is present. If it doesn't come from the PMM, then
			// it's MMIO and it's not tracked.
			if (*Pte & MM_PTE_ISFROMPMM)
			{
				// Free the physical page.
				MMPFN Pfn = MmPhysPageToPFN(*Pte & MM_PTE_ADDRESSMASK);
				MmFreePhysicalPage(Pfn);
			}
		}
		else if (*Pte != 0)
		{
			// If the pte is not zero, then it must have been committed. 
			// Otherwise it's an unknown type which we don't know how to handle.
			ASSERT(*Pte & MM_DPTE_COMMITTED);
		}
		
		*Pte = MM_DPTE_DECOMMITTED;
		
		Pte++;
		CurrentVa += PAGE_SIZE;
		i++;
	}
	
	// Finally, issue a TLB shootdown.
	MmIssueTLBShootDown(StartVa, SizePages);
	
	MmUnlockSpace(Ipl, StartVa);
}

// DEBUG
void MmDebugDumpVad()
{
	PMMVAD_LIST VadList = MmLockVadList();
	
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
	
	DbgPrint("VAD              Start            End              Commit Priv Prot Object");
	if (!Entry)
		DbgPrint("There are no VADs.");
	
	while (Entry)
	{
		PMMVAD Vad = (PMMVAD) Entry;
		
		DbgPrint("%p %p %p %6d %4d %c%c%c  %p",
			Vad,
			Vad->Node.StartVa,
			Vad->Node.StartVa + Vad->Node.Size * PAGE_SIZE,
			Vad->Flags.Committed,
			Vad->Flags.Private,
			(Vad->Flags.Protection & PAGE_READ)    ? 'R' : '-',
			(Vad->Flags.Protection & PAGE_WRITE)   ? 'W' : '-',
			(Vad->Flags.Protection & PAGE_EXECUTE) ? 'X' : '-',
			Vad->Mapped.Object
		);
		
		Entry = GetNextEntryRbTree(Entry);
	}
	
	MmUnlockVadList(VadList);
}
