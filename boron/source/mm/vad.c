/***
	The Boron Operating System
	Copyright (C) 2024-2025 iProgramInCpp

Module name:
	mm/vad.c
	
Abstract:
	Defines functions pertaining to the VAD (Virtual Address
	Descriptor) list of a process.
	
Author:
	iProgramInCpp - 18 August 2024
***/
#include "mi.h"
#include <ex.h>

MMVAD_LIST MiSystemVadList;

PMMVAD_LIST MmLockVadListProcess(PEPROCESS Process)
{
	MiLockVadList(&Process->VadList);
	return &Process->VadList;
}

void MmInitializeVadList(PMMVAD_LIST VadList)
{
	KeInitializeMutex(&VadList->Mutex, MM_VAD_MUTEX_LEVEL);
	InitializeRbTree(&VadList->Tree);
}

void MmInitializeSystemVadList()
{
	// Initialize the kernel's VAD list.
	MmInitializeVadList(&MiSystemVadList);
}

void MmUnlockVadList(PMMVAD_LIST VadList)
{
	KeReleaseMutex(&VadList->Mutex);
}

// Initializes and inserts a VAD.  If a VAD pointer was not provided
// it also allocates one from pool space.
//
// The StartAddress and SizePages are only used if the VAD doesn't exist.
//
// NOTE: This locks the VAD list and unlocks it if UnlockAfter is true.
//
// NOTE: The BackingObject must be a valid MAPPABLE_OBJECT.
BSTATUS MiInitializeAndInsertVad(
	PMMVAD_LIST VadList,
	PMMVAD* InOutVad,
	void* StartAddress,
	size_t SizePages,
	int AllocationType,
	int Protection,
	bool UnlockAfter,
	void* BackingObject,
	size_t SectionOffset,
	PMMVIEW ViewOverride,
	uintptr_t ViewOffset
)
{
	// If a VAD was not provided then this VAD needs to be allocated
	BSTATUS Status;
	bool VadAllocated = false;
	PMMVAD Vad = InOutVad ? *InOutVad : NULL;
	PMMVIEW View = ViewOverride ? ObReferenceObjectByPointer(ViewOverride) : NULL;
	
	if (!ViewOverride)
		ViewOffset = 0;
	
	if (!Vad)
	{
		Vad = MmAllocatePool(POOL_NONPAGED, sizeof(MMVAD));
		if (!Vad)
		{
			if (View) {
				ObDereferenceObject(View);
			}
			return STATUS_INSUFFICIENT_MEMORY;
		}
		
		Vad->Node.StartVa = (uintptr_t) StartAddress;
		Vad->Node.Size = SizePages;
		
		VadAllocated = true;
	}
	
	MiLockVadList(VadList);
	
	if (!View)
	{
		// Try to allocate a view associated with this VAD.
		Status = MmCreateView(
			BackingObject,
			SectionOffset,
			SizePages,
			~AllocationType & MEM_SHARED, // Private
			AllocationType & MEM_COMMIT,  // Commit
			Protection,
			&View
		);
		
		if (FAILED(Status))
		{
			if (VadAllocated)
				MmFreePool(Vad);
			
			if (UnlockAfter)
				MmUnlockVadList(VadList);
			
			return Status;
		}
	}
	
	// Fill in the View property now.
	ASSERT(View);
	Vad->View = View;
	Vad->ViewOffset = ViewOffset;
	
	// Attempt to insert it into the tree.
	if (!InsertItemRbTree(&VadList->Tree, &Vad->Node.Entry))
	{
	#ifdef DEBUG
		// Which status should we return here?  This shouldn't fail here -- we checked
		// for free space already!
		KeCrash("MmInitializeAndInsertVad: Failed to insert VAD into VAD list (address %p)", Vad->Node.StartVa);
	#endif
		
		ObDereferenceObject(View);
		MmUnlockVadList(VadList);
		
		if (VadAllocated)
			MmFreePool(Vad);
		
		return STATUS_INSUFFICIENT_VA_SPACE;
	}
	
	// Reset the other fields of the VAD.
	memset(&Vad->ViewCacheEntry, 0, sizeof(Vad->ViewCacheEntry));
	Vad->ViewCacheLruEntry.Flink = NULL;
	Vad->ViewCacheLruEntry.Blink = NULL;
	
	if (UnlockAfter)
		MmUnlockVadList(VadList);
	
	if (InOutVad)
		*InOutVad = Vad;
	
	return STATUS_SUCCESS;
}

// This is probably the most pain I've ever felt writing a memory manager function.
// Seriously. This is edge case highway right here.
//
// (Just you wait until the Page Table Duplicator(TM) 3000(R)...)
BSTATUS MmOverrideAddressRange(PEPROCESS Process, uintptr_t StartAddress, size_t SizePages, PMMADDRESS_NODE* OutAddrNode)
{
	PMMVAD TempVad1 = MmAllocatePool(POOL_NONPAGED, Process->Heap.ItemSize);
	PMMVAD TempVad2 = MmAllocatePool(POOL_NONPAGED, Process->Heap.ItemSize);
	PMMVAD TempVad3 = MmAllocatePool(POOL_NONPAGED, Process->Heap.ItemSize);
	
	if (!TempVad1 || !TempVad2 || !TempVad3)
	{
		DbgPrint("MmOverrideAddressRange: Failed to allocate temporary VADs.");
		if (TempVad1) MmFreePool(TempVad1);
		if (TempVad2) MmFreePool(TempVad2);
		if (TempVad3) MmFreePool(TempVad3);
		return STATUS_INSUFFICIENT_MEMORY;
	}
	
	memset(TempVad1, 0, Process->Heap.ItemSize);
	memset(TempVad2, 0, Process->Heap.ItemSize);
	memset(TempVad3, 0, Process->Heap.ItemSize);
	
	// As earlier me said (in mm/pt.h), we need to acquire the address space lock first
	// to avoid a lock ordering bug.  Rwlocks DO support recursive ownership in exclusive
	// mode, so just do that.
	KIPL Ipl = MmLockSpaceExclusive(0);
	
	// Then lock the VAD list's lock.  It also supports recursive locking, thankfully.
	PMMVAD_LIST VadList = MmLockVadListProcess(Process);
	
	uintptr_t EndAddress = StartAddress + SizePages * PAGE_SIZE;
	
	// Unmap and/or shrink each VAD inside this range.
	for (PRBTREE_ENTRY VadTreeEntry = GetFirstEntryRbTree(&VadList->Tree);
		VadTreeEntry != NULL;)
	{
		PMMVAD Vad = CONTAINING_RECORD(VadTreeEntry, MMVAD, Node.Entry);
		VadTreeEntry = GetNextEntryRbTree(VadTreeEntry);
		
		// Check if there is any overlap.
		if (EndAddress <= Vad->Node.StartVa || Node_EndVa(&Vad->Node) <= StartAddress)
			continue;
		
		// There is some overlap.  Is it a complete overlap?
		if (StartAddress <= Vad->Node.StartVa && Node_EndVa(&Vad->Node) <= EndAddress)
		{
			// Complete overlap, so the whole VAD will be unmapped.
			//
			// Lock the VAD list again because MiDecommitVad and MiReleaseVad will release it.
			MiLockVadList(VadList);
			MiDecommitVad(VadList, Vad, Vad->Node.StartVa, Vad->Node.Size);
			
			MiLockVadList(VadList);
			MiReleaseVad(Vad);
			continue;
		}
		
		// There is partial overlap.
		
		// The worst case scenario is that the range is entirely engulfed on both sides
		// by the same VAD, in which case we will need to split it up.
		if (Vad->Node.StartVa < StartAddress && EndAddress < Node_EndVa(&Vad->Node))
		{
			// Clean split into two.
			TempVad2->Node.StartVa = EndAddress;
			TempVad2->Node.Size = (Node_EndVa(&Vad->Node) - EndAddress) / PAGE_SIZE;
			
			// Copy properties from the VAD.
			TempVad2->View = ObReferenceObjectByPointer(Vad->View);
			TempVad2->ViewOffset = Vad->ViewOffset + (EndAddress - Vad->Node.StartVa);
			
			// Lock the VAD list again because MiDecommitVad will unlock it.
			//
			// NOTE: Already own the address space rwlock, so no need to worry about lock
			// ordering issues here.
			MiLockVadList(VadList);
			
			// Decommit the VAD at this offset.  Note that this function call will
			// release the lock we just acquired above.
			MiDecommitVad(VadList, Vad, StartAddress, SizePages);
			
			// Then shrink this VAD.
			Vad->Node.Size = (StartAddress - Vad->Node.StartVa) / PAGE_SIZE;
			
			// Now we're done with the original VAD. But we still need to add the second
			// one to the tree. So do it now.
			InsertItemRbTree(&VadList->Tree, &TempVad2->Node.Entry);
			
			// Set TempVad2 to NULL so it won't be freed when this function exits.
			TempVad2 = NULL;
			
			// And also break, since we're pretty sure no more VADs will overlap
			// this range.
			break;
		}
		
		if (Vad->Node.StartVa < StartAddress && Node_EndVa(&Vad->Node) <= EndAddress)
		{
			// Overlap on the left side.
			
			// This is a common theme. You'll see this in the right hand overlap case too.
			//
			// See the above case (complete overlap requiring a split) for an explanation
			// of the locking and decommit scheme.
			
			// Decommit the specified region.
			MiLockVadList(VadList);
			MiDecommitVad(VadList, Vad, StartAddress, (Node_EndVa(&Vad->Node) - StartAddress) / PAGE_SIZE);
			
			// Resize the VAD.
			Vad->Node.Size = (StartAddress - Vad->Node.StartVa) / PAGE_SIZE;
			continue;
		}
		
		if (StartAddress <= Vad->Node.StartVa && EndAddress < Node_EndVa(&Vad->Node))
		{
			// Overlap on the right side.
			size_t Offset = (EndAddress - Vad->Node.StartVa);
			
			// Decommit the specified region.
			MiLockVadList(VadList);
			MiDecommitVad(VadList, Vad, Vad->Node.StartVa, Offset / PAGE_SIZE);
			
			Vad->Node.StartVa += Offset;
			Vad->Node.Size -= Offset / PAGE_SIZE;
			Vad->ViewOffset += Offset;
			
			continue;
		}
		
		KeCrash("MmOverrideAddressRange: Unhandled case (1)");
	}
	
	MmUnlockVadList(VadList);
	
	// Remove or shrink each item in the heap.
	for (PRBTREE_ENTRY HeapTreeEntry = GetFirstEntryRbTree(&Process->Heap.Tree);
		HeapTreeEntry != NULL;)
	{
		// Note that I'm not simply using MmAllocateAddressRange because this causes
		// additional allocations which I'm not too fond of doing.
		
		PMMADDRESS_NODE Node = CONTAINING_RECORD(HeapTreeEntry, MMADDRESS_NODE, Entry);
		HeapTreeEntry = GetNextEntryRbTree(HeapTreeEntry);
		
		// Check if there is any overlap.
		if (EndAddress <= Node->StartVa || Node_EndVa(Node) <= StartAddress)
			continue;
		
		// There is some overlap.  Is it a complete overlap?
		if (StartAddress <= Node->StartVa && Node_EndVa(Node) <= EndAddress)
		{
			// Complete overlap, so the whole heap entry will be removed.
			RemoveItemRbTree(&Process->Heap.Tree, &Node->Entry);
			MmFreePool(Node);
			continue;
		}
		
		// There is partial overlap.
		if (Node->StartVa < StartAddress && EndAddress < Node_EndVa(Node))
		{
			// Clean split into two.
			// Thankfully we don't have to do any shenanigans with decommitting.
			TempVad3->Node.StartVa = EndAddress;
			TempVad3->Node.Size = (Node_EndVa(Node) - EndAddress) / PAGE_SIZE;
			Node->Size = (StartAddress - Node->StartVa) / PAGE_SIZE;
			MmFreeAddressSpace(&Process->Heap, &TempVad3->Node);
			
			// Set TempVad3 to NULL so it won't be freed after its insertion into the tree.
			TempVad3 = NULL;
			
			// It's kind of dangerous to continue, because the node may have been merged,
			// and the next node may have been freed. Restart the tree walk.
			HeapTreeEntry = GetFirstEntryRbTree(&Process->Heap.Tree);
			continue;
		}
		
		if (Node->StartVa < StartAddress && Node_EndVa(Node) <= EndAddress)
		{
			// Overlap on the left side.
			Node->Size = (StartAddress - Node->StartVa) / PAGE_SIZE;
			continue;
		}
		
		if (StartAddress <= Node->StartVa && EndAddress < Node_EndVa(Node))
		{
			// Overlap on the right side.
			size_t Offset = (EndAddress - Node->StartVa);
			Node->StartVa += Offset;
			Node->Size -= Offset / PAGE_SIZE;
			continue;
		}
		
		KeCrash("MmOverrideAddressRange: Unhandled case (2)");
	}
	
	// If the unmap operation covers the 0th page, then exclude it.
	if (StartAddress == 0)
	{
		StartAddress = PAGE_SIZE;
		SizePages--;
	}
	
	// Ensure that the first and last page are still inaccessible to userspace.
	if (StartAddress < MM_FIRST_USER_PAGE)
	{
		StartAddress += PAGE_SIZE;
		SizePages -= 1;
	}
	
	if (StartAddress + SizePages * PAGE_SIZE >= MM_LAST_USER_PAGE)
	{
		SizePages -= 1;
	}
	
	// Set up the address node with the new range.
	TempVad1->Node.StartVa = StartAddress;
	TempVad1->Node.Size = SizePages;
	
	*OutAddrNode = &TempVad1->Node;
	
	MmUnlockSpace(Ipl, 0);
	
	if (TempVad2) MmFreePool(TempVad2);
	if (TempVad3) MmFreePool(TempVad3);
	
	return STATUS_SUCCESS;
}

// NOTE: This does not unlock the VAD list.
BSTATUS MmReserveVirtualMemoryVad(
	size_t SizePages,
	int AllocationType,
	int Protection,
	void* StartAddress,
	void* BackingObject,
	uint64_t SectionOffset,
	PMMVAD* OutVad,
	PMMVAD_LIST* OutVadList
)
{
	PEPROCESS Process = PsGetAttachedProcess();
	PMMADDRESS_NODE AddrNode;
	
	BSTATUS Status;
	
	PMMVIEW View;
	Status = MmCreateView(
		BackingObject,
		SectionOffset,
		SizePages,
		~AllocationType & MEM_SHARED, // Private
		AllocationType & MEM_COMMIT,  // Commit
		Protection, // CommitPermissions
		&View
	);
	
	if (FAILED(Status))
		return Status;

	// Depending on the allocation type, perform different operations.
	if ((AllocationType & (MEM_FIXED | MEM_OVERRIDE)) == (MEM_FIXED | MEM_OVERRIDE)) {
		Status = MmOverrideAddressRange(Process, (uintptr_t) StartAddress, SizePages, &AddrNode);
	}
	else if (AllocationType & MEM_FIXED) {
		Status = MmAllocateAddressRange(&Process->Heap, (uintptr_t) StartAddress, SizePages, &AddrNode);
	}
	else {
		Status = MmAllocateAddressSpace(&Process->Heap, SizePages, AllocationType & MEM_TOP_DOWN, &AddrNode);
	}
	
	if (FAILED(Status))
	{
		ObDereferenceObject(View);
		return Status;
	}
	
	PMMVAD Vad = (PMMVAD) AddrNode;
	
	Status = MiInitializeAndInsertVad(
		&Process->VadList,
		&Vad,
		NULL,  // StartAddress (specified in VAD)
		0,     // SizePages (specified in VAD)
		0,     // AllocationType (specified in the view)
		0,     // Protection (specified in the view)
		false, // UnlockAfter
		NULL,  // BackingObject (specified in view)
		0,     // SectionOffset (specified in view)
		View,  // ViewOverride (the view)
		0      // ViewOffset (no offset into the view)
	);
	
	ObDereferenceObject(View);
	
	// MiInitializeAndInsertVad has no reason to fail at this point.
	if (FAILED(Status)) {
		KeCrash("TODO: Fix this -- MiInitializeAndInsertVad failed: %s", RtlGetStatusString(Status));
	}
	
	*OutVad = Vad;
	*OutVadList = &Process->VadList;
	return Status;
}

BSTATUS MiUnmapVirtualMemoryPartial(uintptr_t StartAddress, size_t SizePages)
{
	PEPROCESS Process = PsGetAttachedProcess();
	PMMADDRESS_NODE AddrNode;
	
	if (!MmIsAddressRangeValidPages(StartAddress, SizePages, MODE_USER))
		return STATUS_INVALID_PARAMETER;
	
	BSTATUS Status = MmOverrideAddressRange(Process, StartAddress, SizePages, &AddrNode);
	if (FAILED(Status))
	{
		// I know it's kind of unintuitive that a memory map operation can fail
		// because of out of memory, but whatever. It's not like we have a choice.
		return Status;
	}
	
	Status = MmFreeAddressSpace(&Process->Heap, AddrNode);
	ASSERT(SUCCEEDED(Status));
	
	return Status;
}

// Reserves a range of virtual memory.
BSTATUS MmReserveVirtualMemory(size_t SizePages, void** InOutAddress, int AllocationType, int Protection)
{
	if (Protection & ~(PAGE_READ | PAGE_WRITE | PAGE_EXECUTE))
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & ~(MEM_RESERVE | MEM_COMMIT | MEM_SHARED | MEM_TOP_DOWN | MEM_FIXED | MEM_OVERRIDE))
		return STATUS_INVALID_PARAMETER;
	
	void* StartVa = *InOutAddress;
	
	PMMVAD Vad;
	PMMVAD_LIST VadList;
	BSTATUS Status = MmReserveVirtualMemoryVad(
		SizePages,
		AllocationType,
		Protection,
		StartVa,
		NULL, // BackingObject
		0,    // SectionOffset
		&Vad,
		&VadList
	);
	
	if (FAILED(Status))
		return Status;
	
	*InOutAddress = (void*) Vad->Node.StartVa;
	ASSERT(PAGE_ALIGNED(Vad->Node.StartVa));
	MmUnlockVadList(VadList);
	return STATUS_SUCCESS;
}

// Cleans up all of the references to this VAD, including now-stale
// PTEs and the object reference that this VAD holds.
void MiCleanUpVad(PMMVAD Vad)
{
	// Decommit the view on the required region, if needed.
	ASSERT(Vad->View);
	MmDecommitView(Vad->View, Vad->ViewOffset, Vad->Node.Size);
	
	// Zero out all of the PTEs.
	KIPL Ipl = MmLockSpaceExclusive(Vad->Node.StartVa);
	MiResetRegionPtes(
		Vad->Node.StartVa,
		Vad->Node.Size,
		false, // ShootdownRange
		false  // OnlyMarkAsReadOnly
	);
	
	MiFreeUnusedMappingLevelsInCurrentMap(Vad->Node.StartVa, Vad->Node.Size);
	
	// Issue a TLB shootdown request covering the whole area.
	MmIssueTLBShootDown(Vad->Node.StartVa, Vad->Node.Size, MmGetTargetProcessForShootdown(Vad->Node.StartVa));
	
	MmUnlockSpace(Ipl, Vad->Node.StartVa);
	
	// Remove the reference to the view.
	ObDereferenceObject(Vad->View);
	Vad->View = NULL;
}

// Releases a range of virtual memory represented by a VAD.
// The range must be entirely decommitted before it is de-reserved.
// The VAD list lock must be held before calling the function.
void MiReleaseVad(PMMVAD Vad)
{
	PEPROCESS Process = PsGetAttachedProcess();

	// Step 1.  Remove the VAD from the VAD list tree.
	RemoveItemRbTree(&Process->VadList.Tree, &Vad->Node.Entry);
	MmUnlockVadList(&Process->VadList);
	
	// Step 2.  Clean up this VAD after it's freed.
	MiCleanUpVad(Vad);
	
	// Step 3. Finally, add the range into the heap/free list.
	BSTATUS Status = MmFreeAddressSpace(&Process->Heap, &Vad->Node);
	if (FAILED(Status))
	{
		// NOTE: This should never happen.
		//
		// Why? Because there's no way for this region to be used if it's
		// not part of either the heap or the VAD list.
		KeCrash("MmDereserveVad: Failed to insert VAD into free-heap (address %p)", Vad->Node.StartVa);
		Status = STATUS_UNIMPLEMENTED;
	}
}

// This releases a range of virtual memory allocated using MmReserveVirtualMemoryVad.
// The address must point to the base of the region.
//
// The old comment said "Note that the entire range must have been decommitted first."
// but this doesn't matter any more.
BSTATUS MmReleaseVirtualMemory(void* Address)
{
	uintptr_t AddressI = (uintptr_t) Address;
	
	PMMVAD_LIST VadList = MmLockVadListProcess(PsGetAttachedProcess());
	
	// Look up the item in the VAD.
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&VadList->Tree, AddressI);
	
	if (!Entry)
	{
		// No entry found.
		MmUnlockVadList(VadList);
		return STATUS_MEMORY_NOT_RESERVED;
	}
	
	PMMVAD Vad = (PMMVAD) Entry;
	if (Vad->Node.StartVa != AddressI)
	{
		// The virtual address of the VAD does not match the address
		// passed into the region.
		MmUnlockVadList(VadList);
		return STATUS_VA_NOT_AT_BASE;
	}
	
	// It does! This means that the region can be dereserved with
	// this function.  Note that it does the job of unlocking the
	// VAD list.
	MiReleaseVad(Vad);
	return STATUS_SUCCESS;
}

PMMVAD MmLookUpVadByAddress(PMMVAD_LIST VadList, uintptr_t Address)
{
	// Look up the item in the VAD.
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&VadList->Tree, Address);
	
	if (!Entry)
	{
		// No entry found.
		return NULL;
	}
	
	PMMVAD Vad = (PMMVAD) Entry;
	
	// If the address is outside of the range of this VAD, then it doesn't
	// correspond to it. (In that case, really, LookUpItemApproximateRbTree
	// failed, since it's supposed to return items whose key is the greatest,
	// yet <= the starting address).
	//
	// Also check if the address is on the other side of the range.
	if (Address < Vad->Node.StartVa ||
		Address >= Vad->Node.StartVa + Vad->Node.Size * PAGE_SIZE)
	{
		// Nope, this VAD doesn't correspond to this region.
		return NULL;
	}
	
	return Vad;
}

#ifdef DEBUG
void MmDumpVadList(PMMVAD_LIST VadList)
{
	if (!VadList)
		VadList = &PsGetAttachedProcess()->VadList;
	
	MiLockVadList(VadList);
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);

	DbgPrint("*** dumping VadList %p", VadList);
	for (; Entry != NULL; Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		DbgPrint("\tAddr: %p\tStart: %p\tSize: %zu", Vad, (void*)Vad->Node.StartVa, Vad->Node.Size);
	}

	MmUnlockVadList(VadList);
}
#endif

// Decommits a range of virtual memory in system space.
void MiDecommitVadInSystemSpace(PMMVAD Vad)
{
	MiLockVadList(&MiSystemVadList);
	MiDecommitVad(&MiSystemVadList, Vad, Vad->Node.StartVa, Vad->Node.Size);
}
