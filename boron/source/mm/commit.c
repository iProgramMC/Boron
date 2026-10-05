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
#include "mi.h"
#include <ex.h>

//
// Checks if the whole range is committed in the PTEs.
//
// NOTE: If Vad->Flags.Committed == 1, DO NOT call this function,
// as it will likely say that the range was not committed.
//
// NOTE: The address space lock MUST have been acquired.
//

// TODO 5/10/26: Remove this, it won't work correctly any more

bool MiIsEntireRangeCommittedNoVad(uintptr_t StartVa, size_t SizePages)
{
	PMMPTE Pte = MmGetPteLocation(StartVa);
	uintptr_t CurrentVa = StartVa;
	for (size_t i = 0; i < SizePages; )
	{
		// If the Pte address crossed into a new page, or if the range is equal to zero,
		// we need to check if that new page is valid.
		if (i == 0 || ((uintptr_t)Pte & (PAGE_SIZE - 1)) == 0)
		{
			if (!MmCheckPteLocation(CurrentVa, false))
			{
				// There is no PTE page.  That means this PTE was not committed.
				//
				// To clarify, the VAD backing this range MUST have Vad->Flags.Committed == 0.
				return false;
			}
		}
		
		// The PTE is accessible.
		if ((!MmIsPresentPte(*Pte) && !MmIsCommittedPte(*Pte)) || MmIsDecommittedPte(*Pte))
		{
			// Not committed.
			return false;
		}
		
		// No overlap, move right along.
		i++;
		Pte++;
		CurrentVa += PAGE_SIZE;
	}
	
	return true;
}

//
// Commits a range of virtual memory, with anonymous pages.
//
// The entire memory region must be uncommitted.
//
BSTATUS MmCommitVirtualMemory(uintptr_t StartVa, size_t SizePages, int Protection)
{
	// Check if the provided address range is valid.
	if (!MmIsAddressRangeValidPages(StartVa, SizePages, MODE_USER))
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
	
	// Just commit the view...
	BSTATUS Status = MmCommitView(Vad->View, StartVa - Vad->Node.StartVa, SizePages, Protection);
	
	MmUnlockVadList(VadList);
	
	return Status;
}

// Decommits a range of virtual memory.
BSTATUS MmDecommitVirtualMemory(uintptr_t StartVa, size_t SizePages)
{
	// Check if the provided address range is valid.
	if (!MmIsAddressRangeValidPages(StartVa, SizePages, MODE_USER) || SizePages == 0)
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

// TODO: Port this behavior over to the view object.
static BSTATUS MmpSetPageAsModifiedIfNeeded(
	MMPTE Pte,
	uintptr_t Va,
	void* VadMappedObject,
	uintptr_t VadStartVa,
	uint64_t VadSectionOffset)
{
	// If the PTE is not dirty, then don't do anything.
	if (!MmIsModifiedPte(Pte))
		return STATUS_SUCCESS;
	
	// If the VAD does not refer to a mapped object, then we don't need
	// to do anything.  Anonymously allocated pages don't require tracking
	// their modified state.
	if (!VadMappedObject)
		return STATUS_SUCCESS;
	
	const uintptr_t PageMask = ~(PAGE_SIZE - 1);
	uint64_t SectionOffset = ((Va & PageMask) - VadStartVa + VadSectionOffset) / PAGE_SIZE;
	return MmSetPageModifiedMappable(VadMappedObject, SectionOffset);
}

// This part of MmDecommitVirtualMemory has been split into a separate function because
// this is also referenced by MmTearDownVadList().
void MiDecommitVad(PMMVAD_LIST VadList, PMMVAD Vad, size_t StartVa, size_t SizePages)
{
	MmDecommitView(Vad->View, StartVa - Vad->Node.StartVa, SizePages);
	MmUnlockVadList(VadList);
}

// DEBUG
void MmDebugDumpVad()
{
	PMMVAD_LIST VadList = MmLockVadList();
	
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
	
	DbgPrint("VAD              Start            End              ViewObject");
	if (!Entry)
		DbgPrint("There are no VADs.");
	
	while (Entry)
	{
		PMMVAD Vad = (PMMVAD) Entry;
		
		DbgPrint("%p %p %p %p",
			Vad,
			Vad->Node.StartVa,
			Vad->Node.StartVa + Vad->Node.Size * PAGE_SIZE,
			Vad->View
		);
		
		Entry = GetNextEntryRbTree(Entry);
	}
	
	MmUnlockVadList(VadList);
}
