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

// NOTE: The address space lock MUST be held during this.
void MiResetRegionPtes(
	uintptr_t StartVa,
	size_t SizePages,
	bool ShootdownRange,
	bool OnlyMarkAsReadOnly,
	PMMVIEW View,
	uintptr_t ViewBaseVa
)
{
	MMPTE CommittedButNotFaultedInPte = MmBuildZeroPte();
	
	// Now go through each page and put all of the allocated PFNs inside.
	for (size_t i = 0; i < SizePages; i++)
	{
		uintptr_t Address = StartVa + i * PAGE_SIZE;
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
		if (!MmIsPresentPte(Pte))
		{
			// TODO: What if this is a paged-out PTE? Or some other kind of PTE that is
			// not committed/decommitted?
			//
			// 5/10/26: Actually we probably don't need to do anything here because
			// such a PTE would have to go through the PF handler anyway and our only
			// goal is to reset this PTE to its unfaulted/read-only state.
			continue;
		}
		
		if (MmIsModifiedPte(Pte) && View)
		{
			MiSetPageModifiedView(View, Address - ViewBaseVa);
		}
		
		if (OnlyMarkAsReadOnly)
		{
			*PtePtr = MmReadOnlyPte(Pte);
		}
		else
		{
			// The PTE has to come from the PMM, I can't explain it otherwise.
			ASSERT(MmIsFromPmmPte(Pte));
			
			MMPFN Pfn = MmGetPfnPte(Pte);
			MmFreePhysicalPage(Pfn);
			*PtePtr = CommittedButNotFaultedInPte;
		}
	}
	
	if (ShootdownRange)
	{
		MmIssueTLBShootDown(StartVa, SizePages, MmGetTargetProcessForShootdown(StartVa));
	}
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

static BSTATUS MmpCloneAllViews(PEPROCESS DestinationProcess, PEPROCESS SourceProcess, size_t HeapItemSize)
{
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&SourceProcess->VadList.Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD SourceVad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		PMMVAD DestinationVad = MmAllocatePool(POOL_NONPAGED, HeapItemSize);
		if (!DestinationVad)
			return STATUS_INSUFFICIENT_MEMORY;
		
		// memcpy all the details from the source VAD, but use a different view pointer.
		memcpy(DestinationVad, SourceVad, HeapItemSize);
		
		PMMVIEW NewView;
		BSTATUS Status;
		
		Status = MmCloneView(SourceVad->View, &NewView);
		if (FAILED(Status))
		{
			MmFreePool(DestinationVad);
			return Status;
		}
		
		DestinationVad->View = NewView;
		
		InsertItemRbTree(&DestinationProcess->VadList.Tree, &DestinationVad->Node.Entry);
	}
	
	return STATUS_SUCCESS;
}

static void MmpCopyOnWriteAllViews(PEPROCESS SourceProcess)
{
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&SourceProcess->VadList.Tree);
		Entry != NULL;
		Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		MmCopyOnWriteView(Vad->View);
		MiResetRegionPtes(
			Vad->Node.StartVa,
			Vad->Node.Size,
			true,
			true,
			Vad->View,
			Vad->Node.StartVa - Vad->ViewOffset
		);
	}
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
	
	// Clone the heap into the destination process.
	DestHeap->ItemSize = SrcHeap->ItemSize;
	Status = MmpCloneAddressNodes(&DestHeap->Tree, &SrcHeap->Tree, SrcHeap->ItemSize);
	if (FAILED(Status))
		goto Exit2;
	
	// Clone all the VADs from the source process.
	Status = MmpCloneAllViews(DestinationProcess, SourceProcess, SrcHeap->ItemSize);
	if (FAILED(Status))
		goto Exit2;
	
	// Mark all mapped views as copy on write on the source's side too.
	MmpCopyOnWriteAllViews(SourceProcess);
	
	// Success
	if (DestHeapOnlyNode) {
		MmFreePool(DestHeapOnlyNode);
	}
	
	goto Exit;
	
Exit2:
	// Free every VAD and heap item.
	for (PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&DestVadList->Tree);
		Entry != NULL;
		Entry = GetFirstEntryRbTree(&DestVadList->Tree))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		RemoveItemRbTree(&DestVadList->Tree, &Vad->Node.Entry);
		if (Vad->View)
			ObDereferenceObject(Vad->View);
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
