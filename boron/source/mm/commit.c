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
	if ((Protection & ~(MM_PTE_READWRITE | MM_PTE_NOEXEC)) != 0)
		return STATUS_INVALID_PARAMETER;
	
	// Check if the provided address range is valid.
	if (!MmIsAddressRangeValid(StartVa, StartVa + SizePages * PAGE_SIZE, MODE_USER))
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
		return STATUS_CONFLICTING_ADDRESSES;
	}
	
	MmUnlockVadList(VadList);
	
	// If this range is committed, check if the PTEs are marked decommitted.
	// Otherwise, check if they are null.
	BSTATUS Status = STATUS_SUCCESS;
	KIPL Ipl = MmLockSpaceShared(StartVa);
	
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
				if (Vad->Flags.Committed)
				{
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
			Status = STATUS_CONFLICTING_ADDRESSES;
			break;
		}
		
		// If the VAD is marked committed and the page we're looking at hasn't been
		// decommitted, there is overlap.
		if (Vad->Flags.Committed && (~*Pte & MM_DPTE_DECOMMITTED))
		{
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
				return STATUS_INSUFFICIENT_MEMORY;
			}
		}
		
		*Pte = MM_DPTE_COMMITTED | PteFlags;
	}
	
	// Okay, everything's committed. Success!
	MmUnlockSpace(Ipl, StartVa);
	return STATUS_SUCCESS;
}
