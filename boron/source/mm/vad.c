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
BSTATUS MiInitializeAndInsertVad(
	PMMVAD_LIST VadList,
	PMMVAD* InOutVad,
	void* StartAddress,
	size_t SizePages,
	int AllocationType,
	int Protection,
	bool UnlockAfter
)
{
	// If a VAD was not provided then this VAD needs to be allocated
	bool Allocated = false;
	PMMVAD Vad = InOutVad ? *InOutVad : NULL;
	
	if (!Vad)
	{
		Vad = MmAllocatePool(POOL_NONPAGED, sizeof(MMVAD));
		if (!Vad)
			return STATUS_INSUFFICIENT_MEMORY;
		
		Vad->Node.StartVa = (uintptr_t) StartAddress;
		Vad->Node.Size = SizePages;
		
		Allocated = true;
	}
	
	MiLockVadList(VadList);
	
	if (!InsertItemRbTree(&VadList->Tree, &Vad->Node.Entry))
	{
		// Which status should we return here?
	#ifdef DEBUG
		KeCrash("MmInitializeAndInsertVad: Failed to insert VAD into VAD list (address %p)", Vad->Node.StartVa);
	#endif
		MmUnlockVadList(VadList);
		
		if (Allocated)
			MmFreePool(Vad);
		
		return STATUS_INSUFFICIENT_VA_SPACE;
	}
	
	// Clear all the fields in the VAD.
	Vad->Flags.LongFlags  = 0;
	Vad->Mapped.Object    = NULL;
	Vad->SectionOffset    = 0;
	Vad->Flags.Cow        =  (AllocationType & MEM_COW) != 0;
	Vad->Flags.Committed  =  (AllocationType & MEM_COMMIT) != 0;
	Vad->Flags.Private    = (~AllocationType & MEM_SHARED) != 0;
	Vad->Flags.Protection = Protection;
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
		VadTreeEntry != NULL;
		VadTreeEntry = GetNextEntryRbTree(VadTreeEntry))
	{
		PMMVAD Vad = CONTAINING_RECORD(VadTreeEntry, MMVAD, Node.Entry);
		
		// Check if there is any overlap.
		if (EndAddress <= Vad->Node.StartVa || Node_EndVa(&Vad->Node) <= StartAddress)
			continue;
		
		// There is some overlap.  Is it a complete overlap?
		if (StartAddress <= Vad->Node.StartVa && Node_EndVa(&Vad->Node) <= EndAddress)
		{
			// Complete overlap, so the whole VAD will be unmapped.
			//
			// Lock the VAD list again because MiReleaseVad will acquire it.
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
			TempVad2->Flags = Vad->Flags;
			TempVad2->Mapped.Object = Vad->Mapped.Object;
			
			if (Vad->Mapped.Object)
			{
				ObReferenceObjectByPointer(Vad->Mapped.Object);
			
				// Calculate a new section offset.
				TempVad2->SectionOffset = Vad->SectionOffset + (EndAddress - Vad->Node.StartVa);
			}
			
			// TODO: Temporarily set this VAD's Committed flag to false, so that
			// MiDecommitVad won't generate new page tables for the decommitted region.
			bool WasCommitted = Vad->Flags.Committed;
			Vad->Flags.Committed = false;
			
			// Lock the VAD list again because MiDecommitVad will unlock it.
			//
			// NOTE: Already own the address space rwlock, so no need to worry about lock
			// ordering issues here.
			MiLockVadList(VadList);
			
			// Decommit the VAD at this offset.  Note that this function call will
			// release the lock we just acquired above.
			//
			// Also note that normally this would create page tables for the region,
			// which we don't want. (we'd be very hard-pressed to fail right now!)
			// So we do the trick of turning off the committed flag while decommitting.
			// Note that any extant page faults will be waiting on either the VAD lock
			// or the address space lock, both of which we own, so they won't see this
			// intermediate state.
			MiDecommitVad(VadList, Vad, StartAddress, SizePages);
			
			// Then shrink this VAD.
			Vad->Node.Size = (StartAddress - Vad->Node.StartVa) / PAGE_SIZE;
			
			// Finally, restore the committed flag.
			Vad->Flags.Committed = WasCommitted;
			
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
			bool WasCommitted = Vad->Flags.Committed;
			Vad->Flags.Committed = false;
			
			// Decommit the specified region.
			MiLockVadList(VadList);
			MiDecommitVad(VadList, Vad, StartAddress, (Node_EndVa(&Vad->Node) - StartAddress) / PAGE_SIZE);
			
			// Resize the VAD.
			Vad->Node.Size = (StartAddress - Vad->Node.StartVa) / PAGE_SIZE;
			
			Vad->Flags.Committed = WasCommitted;
			continue;
		}
		
		if (StartAddress <= Vad->Node.StartVa && EndAddress < Node_EndVa(&Vad->Node))
		{
			// Overlap on the right size.
			
			bool WasCommitted = Vad->Flags.Committed;
			Vad->Flags.Committed = false;
			
			size_t Offset = (EndAddress - Vad->Node.StartVa);
			
			// Decommit the specified region.
			MiLockVadList(VadList);
			MiDecommitVad(VadList, Vad, Vad->Node.StartVa, Offset / PAGE_SIZE);
			
			Vad->Node.StartVa += Offset;
			Vad->Node.Size -= Offset / PAGE_SIZE;
			if (Vad->Mapped.Object)
				Vad->SectionOffset += Offset;
			
			Vad->Flags.Committed = WasCommitted;
			continue;
		}
		
		KeCrash("MmOverrideAddressRange: Unhandled case (1)");
	}
	
	// Remove or shrink each item in the heap.
	for (PRBTREE_ENTRY HeapTreeEntry = GetFirstEntryRbTree(&Process->Heap.Tree);
		HeapTreeEntry != NULL;
		HeapTreeEntry = GetNextEntryRbTree(HeapTreeEntry))
	{
		// Note that I'm not simply using MmAllocateAddressRange because this causes
		// additional allocations which I'm not too fond of doing.
		
		PMMADDRESS_NODE Node = CONTAINING_RECORD(HeapTreeEntry, MMADDRESS_NODE, Entry);
		
		// Check if there is any overlap.
		if (EndAddress <= Node->StartVa || Node_EndVa(Node) <= StartAddress)
			continue;
		
		if (Node->StartVa < StartAddress && EndAddress < Node_EndVa(Node))
		{
			// Clean split into two.
			// Thankfully we don't have to do any shenanigans with decommitting.
			TempVad3->Node.StartVa = EndAddress;
			TempVad3->Node.Size = (Node_EndVa(Node) - EndAddress) / PAGE_SIZE;
			Node->Size = (StartAddress - Node->StartVa) / PAGE_SIZE;
			InsertItemRbTree(&Process->Heap.Tree, &TempVad3->Node.Entry);
			
			// Set TempVad3 to NULL so it won't be freed after its insertion into the tree.
			TempVad3 = NULL;
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
	
	// Set up the address node with the new range.
	TempVad1->Node.StartVa = StartAddress;
	TempVad1->Node.Size = SizePages;
	
	*OutAddrNode = &TempVad1->Node;
	
	MmUnlockVadList(VadList);
	MmUnlockSpace(Ipl, 0);
	
	if (TempVad2) MmFreePool(TempVad2);
	if (TempVad3) MmFreePool(TempVad3);
	
	return STATUS_SUCCESS;
}

// NOTE: This does not unlock the VAD list.
BSTATUS MmReserveVirtualMemoryVad(size_t SizePages, int AllocationType, int Protection, void* StartAddress, PMMVAD* OutVad, PMMVAD_LIST* OutVadList)
{
	PEPROCESS Process = PsGetAttachedProcess();
	PMMADDRESS_NODE AddrNode;
	
	BSTATUS Status;

	if ((AllocationType & (MEM_FIXED | MEM_OVERRIDE)) == (MEM_FIXED | MEM_OVERRIDE))
		Status = MmOverrideAddressRange(Process, (uintptr_t) StartAddress, SizePages, &AddrNode);
	else if (AllocationType & MEM_FIXED)
		Status = MmAllocateAddressRange(&Process->Heap, (uintptr_t) StartAddress, SizePages, &AddrNode);
	else
		Status = MmAllocateAddressSpace(&Process->Heap, SizePages, AllocationType & MEM_TOP_DOWN, &AddrNode);
	
	if (FAILED(Status))
		return Status;
	
	PMMVAD Vad = (PMMVAD) AddrNode;
	
	Status = MiInitializeAndInsertVad(&Process->VadList, &Vad, NULL, 0, AllocationType, Protection, false);
	
	if (FAILED(Status))
		KeCrash("TODO: Fix this");
	
	*OutVad = Vad;
	*OutVadList = &Process->VadList;
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
	BSTATUS Status = MmReserveVirtualMemoryVad(SizePages, AllocationType, Protection, StartVa, &Vad, &VadList);
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
	// Zero out all of the PTEs.
	KIPL Ipl = MmLockSpaceExclusive(Vad->Node.StartVa);
	
	uintptr_t CurrentVa = Vad->Node.StartVa;
	PMMPTE Pte = MmGetPteLocation(CurrentVa);
	for (size_t i = 0; i < Vad->Node.Size; i++)
	{
		if (i == 0 || ((uintptr_t)Pte & 0xFFF) == 0)
		{
			if (!MmCheckPteLocation(CurrentVa, false))
			{
				// Nothing to zero out here.
				size_t OldVa = CurrentVa;
				CurrentVa = (CurrentVa + PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE)) & ~(PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE) - 1);
				Pte = MmGetPteLocation(CurrentVa);
				i += (CurrentVa - OldVa) / PAGE_SIZE;
				continue;
			}
		}
		
		// Assume that the page is NOT present.
		ASSERT(~*Pte & MM_PTE_PRESENT);
		
		*Pte = 0;
		Pte++;
		CurrentVa += PAGE_SIZE;
	}
	
	MiFreeUnusedMappingLevelsInCurrentMap(Vad->Node.StartVa, Vad->Node.Size);
	
	// Issue a TLB shootdown request covering the whole area. 
	MmIssueTLBShootDown(Vad->Node.StartVa, Vad->Node.Size);
	
	MmUnlockSpace(Ipl, Vad->Node.StartVa);
	
	// Remove the reference to the object if needed.
	if (Vad->Mapped.Object)
	{
		ObDereferenceObject(Vad->Mapped.Object);
		Vad->Mapped.Object = NULL;
	}
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
// Note that the entire range must have been decommitted first.
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
	
	ASSERT(!Vad->Flags.Committed);
	
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
		DbgPrint("\tStart: %p\tEnd: %zu", (void*)Vad->Node.StartVa, Vad->Node.Size);
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
