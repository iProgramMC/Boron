/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/clone.c
	
Abstract:
	This module contains the implementation of the MmCloneAddressSpace
	function.  This function clones the address space of a process, and
	modifies the original process' address space by inserting overlays
	on private mappings.
	
Author:
	iProgramInCpp - 12 December 2025
***/

#include "mi.h"

static BSTATUS MmpChangeAnonymousMemoryIntoSections(PMMVAD_LIST VadList)
{
	// Note:
	// If one of the sections fails, then we DON'T really need to roll this
	// part back, which is great because I really don't want to write all of
	// that code.
	BSTATUS Status = STATUS_SUCCESS;
	
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		// Does it have a mapped object?
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		if (Vad->MappedObject)
		{
			// Yes, so we don't need to turn it into anything.
			continue;
		}
		
		// Anonymous mapping.  Try and create a section object.  This section
		// object will represent the new mapping.  Eventually it'll also have
		// a CoW overlay attached to it, although it's not being done in this
		// step.
		PMMSECTION Section = NULL;
		Status = MmCreateAnonymousSectionObject(&Section, Vad->Node.Size * PAGE_SIZE);
		
		if (FAILED(Status))
			break;
		
		// Now go through each page and put all of the allocated PFNs inside.
		for (size_t i = 0; i < Vad->Node.Size; i++)
		{
			uintptr_t Address = Vad->Node.StartVa + i * PAGE_SIZE;
			PMMPTE PtePtr = MmGetPteLocationCheck(Address, false);
			
			if (!PtePtr)
			{
				// PTE doesn't exist, move along.
				// TODO: skip ALL PTEs within this page.
				continue;
			}
			
			MMPTE Pte = *PtePtr;
			if (Pte & MM_PTE_PRESENT)
			{
				// The PTE has to come from the PMM, I can't explain it otherwise.
				ASSERT(MM_PTE_CHECKFROMPMM(Pte));
				
				MMPFN Pfn = MM_PTE_PFN(Pte);
				
				uint64_t SectionOffset = (Vad->SectionOffset + i * PAGE_SIZE) / PAGE_SIZE;
				Status = MiAssignEntrySection(Section, SectionOffset, Pfn);
				if (FAILED(Status))
				{
					ObDereferenceObject(Section);
					break;
				}
			}
			
			// TODO: What if this is a paged-out PTE? Or some other kind of PTE that is
			// not committed/decommitted?
		}
		
		// The section conversion was successful, so put the section reference in
		// the VAD.  Now, we actually DON'T need to clear the PTEs in this step,
		// since the PFNs remain unchanged.
		Vad->MappedObject = Section;
	}
	
	return Status;
}

// Note: pass NULL to go through every entry.
static void MmpUndoAddedOverlays(PMMVAD_LIST VadList, PRBTREE_ENTRY StopEntry)
{
	// Every private VAD entry up until and except for FailedEntry has been touched.
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
		Entry != StopEntry;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		if (!Vad->Flags.Private)
			continue;
		
		ASSERT(ObGetObjectType(Vad->MappedObject) == MmOverlayObjectType);
		
		PMMOVERLAY Overlay = Vad->MappedObject;
		Vad->MappedObject = ObReferenceObjectByPointer(Overlay->Parent);
		ObDereferenceObject(Overlay);
	}
}

static void MmpReferenceMappedObjects(PMMVAD_LIST VadList)
{
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		ASSERT(Vad->MappedObject && "Seriously ridiculous.");
		
		ObReferenceObjectByPointer(Vad->MappedObject);
	}
}

static BSTATUS MmpAddOverlaysIfNeeded(PMMVAD_LIST VadList, bool WritePTEs)
{
	BSTATUS Status = STATUS_SUCCESS;
	PRBTREE_ENTRY FailedEntry = NULL;
	
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		ASSERT(
			Vad->MappedObject && "You should've done an operation that turned every "
			"existing anonymous VAD into an anon section"
		);
		
		// Shared mappings aren't subject to this conversion.
		if (!Vad->Flags.Private)
			continue;
		
		PMMOVERLAY Overlay = NULL;
		Status = MmCreateOverlayObject(
			&Overlay,
			Vad->MappedObject,
			0
		);
		
		if (FAILED(Status))
		{
			FailedEntry = Entry;
			goto Rollback;
		}
		
		ObDereferenceObject(Vad->MappedObject);
		Vad->MappedObject = Overlay;
	}
	
	// Okay. Currently *EVERY* privately mapped object has been turned into a CoW
	// overlay.  Now remove every privately mapped area of memory from the address
	// space.  It'll be faulted back in through the overlay.
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		ASSERT(Vad->MappedObject && "This is ridiculous.");
		
		if (!Vad->Flags.Private)
			continue;
		
		MMPTE CommittedButNotFaultedInPte = 0;
		if (!Vad->Flags.Committed)
			CommittedButNotFaultedInPte = MM_DPTE_COMMITTED;
		
		// Now go through each page and put all of the allocated PFNs inside.
		for (size_t i = 0; WritePTEs && i < Vad->Node.Size; i++)
		{
			uintptr_t Address = Vad->Node.StartVa + i * PAGE_SIZE;
			PMMPTE PtePtr = MmGetPteLocationCheck(Address, false);
			
			if (!PtePtr)
			{
				// This can happen in two cases:
				// - in VADs where Vad->Flags.Committed = 1, when nobody accessed the
				//   actual VAD in this X MB region, and
				// - in VADs where Vad->Flags.Committed = 0, when nobody committed the
				//   region covered by this X MB of address space.
				//
				// In either case, there is NO information here, so we can just move
				// over this part entirely.
				
				// TODO: skip ALL PTEs within this page.
				continue;
			}
			
			MMPTE Pte = *PtePtr;
			if (Pte & MM_PTE_PRESENT)
			{
				// The PTE has to come from the PMM, I can't explain it otherwise.
				ASSERT(MM_PTE_CHECKFROMPMM(Pte));
				
				MMPFN Pfn = MM_PTE_PFN(Pte);
				MmFreePhysicalPage(Pfn);
				
				*PtePtr = CommittedButNotFaultedInPte;
			}
			
			// TODO: What if this is a paged-out PTE? Or some other kind of PTE that is
			// not committed/decommitted?
		}
	}
	
	if (WritePTEs) {
		KeFlushTLB();
	}
	
	return Status;
	
Rollback:
	MmpUndoAddedOverlays(VadList, FailedEntry);
	if (WritePTEs) {
		KeFlushTLB();
	}
	
	return Status;
}

static BSTATUS MmpCloneAddressNodes(PRBTREE DestTree, PRBTREE SrcTree, size_t ItemSize)
{
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(SrcTree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMADDRESS_NODE AddrNode = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		PMMADDRESS_NODE AddrNodeDest = MmAllocatePool(POOL_NONPAGED, ItemSize);
		if (!AddrNodeDest)
			return STATUS_INSUFFICIENT_MEMORY;
		
		memcpy(AddrNodeDest, AddrNode, ItemSize);
		InsertItemRbTree(DestTree, &AddrNodeDest->Entry);
	}
	
	return STATUS_SUCCESS;
}

static BSTATUS MmpReplicateCommitPtesIfNeeded(PEPROCESS DestinationProcess, PMMVAD_LIST VadList)
{
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		const bool MustFillInCommittedPtes = !Vad->Flags.Committed;
		const bool MustFillInDecommittedPtes = Vad->Flags.Committed;
		
		for (size_t i = 0; i < Vad->Node.Size; i++)
		{
			uintptr_t Address = Vad->Node.StartVa + i * PAGE_SIZE;
			PMMPTE PtePtr = MmGetPteLocationCheck(Address, false);
			
			if (!PtePtr)
			{
				// This is already being imitated, funnily enough.
				continue;
			}
			
			MMPTE DestPte = 0;
			MMPTE Pte = *PtePtr;
			if (Pte & MM_PTE_PRESENT)
			{
				DestPte = MM_DPTE_COMMITTED;
			}
			else
			{
				if (Pte & MM_DPTE_COMMITTED)
					DestPte = MM_DPTE_COMMITTED;
				else if (Pte & MM_DPTE_DECOMMITTED)
					DestPte = MM_DPTE_DECOMMITTED;
			}
			
			if (Pte &&
			    ((MustFillInCommittedPtes && DestPte == MM_DPTE_COMMITTED) ||
				 (MustFillInDecommittedPtes && DestPte == MM_DPTE_DECOMMITTED)))
			{
				// This is really, *really*, inefficient.  We could do this in a different way,
				// or disable this entirely and just force every VAD to be committed.
				DbgPrint("%s: setting PTE for VA %p", __func__, Address);
				PEPROCESS OldProcess = PsSetAttachedProcess(DestinationProcess);
				
				PMMPTE DestPtePtr = MmGetPteLocationCheck(Address, true);
				if (!DestPtePtr)
				{
					PsSetAttachedProcess(OldProcess);
					return STATUS_INSUFFICIENT_MEMORY;
				}
				
				*DestPtePtr = DestPte;
				
				PsSetAttachedProcess(OldProcess);
			}
		}
	}
	
	return STATUS_SUCCESS;
}

// This function clones the current process' address space to another process' address space.
// This is done by iterating through the process' VADs and determining how the new process
// should reference them.
//
// In particular, the following processes are performed:
// - Copy heap and VAD items to the destination process
// - Transparently morph every private anonymous non-object mapping into a copy-on-write mapping
//   of an anonymous section
// - For every private mapping of a section or file object, create a copy-on-write overlay on top
//   of the actual object for both the source and destination processes separately
// - Replicate the committed state of each mapping for each VAD
//
// NOTE: For now, you CANNOT clone an arbitrary process. This always clones the CURRENT
// process. So before attempting to clone, the caller must attach the process they're
// trying to clone, if they aren't cloning their current process.
BSTATUS MmCloneAddressSpace(PEPROCESS DestinationProcess)
{
	BSTATUS Status;
	PMMVAD_LIST DestVadList = &DestinationProcess->VadList;
	PMMHEAP DestHeap = &DestinationProcess->Heap;
	
	PEPROCESS SourceProcess = PsGetAttachedProcess();
	PMMVAD_LIST SrcVadList = &SourceProcess->VadList;
	PMMHEAP SrcHeap = &SourceProcess->Heap;
	
	// According to mm/pt.h, the locking order is address space lock, then VAD lock.
	// Since I can't simply acquire the address space lock using a KeWaitForMultipleObjects
	// call, acquire it first here.
	KIPL SpaceLockIpl = MmLockSpaceExclusive(0);
	
	// Acquire both address spaces' mutexes.
	void* Objects[2] = { &SrcVadList->Mutex, &DestVadList->Mutex };
	Status = KeWaitForMultipleObjects(2, Objects, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, NULL, MODE_KERNEL);
	ASSERT(SUCCEEDED(Status));
	
	// First, ensure that the destination process doesn't have any running
	// threads and existing VADs that we would overwrite.
	if (!IsListEmpty(&DestinationProcess->Pcb.ThreadList))
	{
		Status = STATUS_INVALID_PARAMETER;
		goto Exit;
	}
	
	if (!IsEmptyRbTree(&DestVadList->Tree))
	{
		Status = STATUS_CONFLICTING_ADDRESSES;
		goto Exit;
	}
	
	// Remove the only entry inside the destination heap's tree, if there is any.
	PMMADDRESS_NODE DestHeapOnlyNode = CONTAINING_RECORD(GetFirstEntryRbTree(&DestHeap->Tree), MMADDRESS_NODE, Entry);
	if (DestHeapOnlyNode) {
		RemoveItemRbTree(&DestHeap->Tree, &DestHeapOnlyNode->Entry);
	}
	
	// We need to prepare the source process for symmetric copy-on-write.  To do this, we must ensure
	// that every anonymous memory VAD is turned into a mappable object referencing VAD.
	Status = MmpChangeAnonymousMemoryIntoSections(SrcVadList);
	if (FAILED(Status))
		goto Exit2;
	
	// Clone the heap into the destination process.
	DestHeap->ItemSize = SrcHeap->ItemSize;
	Status = MmpCloneAddressNodes(&DestHeap->Tree, &SrcHeap->Tree, SrcHeap->ItemSize);
	if (FAILED(Status))
		goto Exit2;
	
	// Clone the VADs too.  However, they won't function correctly yet.
	Status = MmpCloneAddressNodes(&DestVadList->Tree, &SrcVadList->Tree, SrcHeap->ItemSize);
	if (FAILED(Status))
		goto Exit2;
	
	// Reference every object referenced in the VADs of the destination process.
	MmpReferenceMappedObjects(DestVadList);
	
	// Add overlays inside both the source and destination.
	Status = MmpAddOverlaysIfNeeded(SrcVadList, true);
	if (FAILED(Status))
		goto Exit3;
	
	Status = MmpAddOverlaysIfNeeded(DestVadList, false);
	if (FAILED(Status))
		goto Exit3;
	
	// Overlays have been added. Now, we still need to replicate the PTEs for each VAD.
	Status = MmpReplicateCommitPtesIfNeeded(DestinationProcess, SrcVadList);
	if (FAILED(Status))
		goto Exit3;
	
	// Success
	if (DestHeapOnlyNode) {
		MmFreePool(DestHeapOnlyNode);
	}
	
	goto Exit;
	
Exit3:
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&DestVadList->Tree);
		Entry != NULL;
		Entry = GetFirstEntryRbTree(&DestVadList->Tree))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		if (Vad->MappedObject)
			ObDereferenceObject(Vad->MappedObject);
	}
	
	MmpUndoAddedOverlays(SrcVadList, NULL);

Exit2:
	// Free every VAD and heap item.
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&DestVadList->Tree);
		Entry != NULL;
		Entry = GetFirstEntryRbTree(&DestVadList->Tree))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		RemoveItemRbTree(&DestVadList->Tree, &Vad->Node.Entry);
		MmFreePool(Vad);
	}
	
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&DestHeap->Tree);
		Entry != NULL;
		Entry = GetFirstEntryRbTree(&DestHeap->Tree))
	{
		PMMADDRESS_NODE AddrNode = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		RemoveItemRbTree(&DestHeap->Tree, &AddrNode->Entry);
		MmFreePool(AddrNode);
	}
	
	// Also reinsert the old heap node, reverting the change we made.
	if (DestHeapOnlyNode) {
		InsertItemRbTree(&DestHeap->Tree, &DestHeapOnlyNode->Entry);
	}
	
Exit:
	MmUnlockVadList(DestVadList);
	MmUnlockVadList(SrcVadList);
	MmUnlockSpace(SpaceLockIpl, 0);
	return Status;
}
