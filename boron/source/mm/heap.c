/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/heap.c
	
Abstract:
	This module implements the heap manager in Boron.  This manages the free
	portions of the virtual address space.
	
Author:
	iProgramInCpp - 22 September 2024
***/
#include "mi.h"

bool MmpCreateRegionHeap(PMMHEAP Heap, uintptr_t StartVa, size_t Size)
{
	PMMADDRESS_NODE Mem = MmAllocatePool(POOL_NONPAGED, Heap->ItemSize);
	if (!Mem)
		return false;

	InitializeRbTreeEntry(&Mem->Entry);
	Mem->StartVa = StartVa;
	Mem->Size = Size;

	InsertItemRbTree(&Heap->Tree, &Mem->Entry);
	return true;
}

void MmInitializeHeap(PMMHEAP Heap, size_t ItemSize, uintptr_t InitialVa, size_t InitialSize)
{
	InitializeRbTree(&Heap->Tree);
	KeInitializeMutex(&Heap->Mutex, 0);
	
	Heap->ItemSize = ItemSize;
	ASSERT(ItemSize >= sizeof(MMADDRESS_NODE));
	
	MmpCreateRegionHeap(Heap, InitialVa, InitialSize);
}

BSTATUS MmAllocateAddressRange(PMMHEAP Heap, uintptr_t Va, size_t SizePages, PMMADDRESS_NODE* OutNode)
{
	BSTATUS Status = STATUS_SUCCESS;
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	
	// Look in the heap for a node that's near this VA.
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&Heap->Tree, Va);
	
	// NOTE: All contiguous nodes are merged together on free.
	
	// This function looks up the biggest element <= the VA.
	// If it doesn't exist then just assume there are conflicting addresses,
	// because they probably are.
	if (!Entry)
	{
	Conflict:
		Status = STATUS_CONFLICTING_ADDRESSES;
		goto Exit;
	}
	
	// Check if the VA and the size are exact.  If so, just return the node.
	PMMADDRESS_NODE Node = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
	if (Node->StartVa == Va && Node->Size == SizePages)
		goto Success;
	
	// Does this requested range even fit?
	if (Node->Size < SizePages)
		goto Conflict;
	
	// Is the ending VA outside of this node's range?
	if (Node_EndVa(Node) < Va + SizePages * PAGE_SIZE)
		// The next node cannot possibly be contiguous with this one
		// because we ensure that doesn't happen on free.
		goto Conflict;
	
	// They are not.  Check if we only need to cut from the right.
	if (Node->StartVa == Va)
	{
		if (!MmpCreateRegionHeap(Heap, Node->StartVa + SizePages, Node->Size - SizePages))
		{
			Status = STATUS_INSUFFICIENT_MEMORY;
			goto Exit;
		}
		
		Node->Size = SizePages;
		goto Success;
	}
	
	uintptr_t RangeVa = Node->StartVa;
	size_t RangeSize = Node->Size;
	
	// Check if we only need to cut from the left.
	if (Node_EndVa(Node) == Va + SizePages * PAGE_SIZE)
	{
		// NOTE: surely we're able to do that, right? Because the entire
		// range is covered by this one node so just chopping off a bit from
		// the start should NOT influence the tree at all.
		Node->StartVa = Node_EndVa(Node) - SizePages * PAGE_SIZE;
		Node->Size = SizePages;
		
		if (!MmpCreateRegionHeap(Heap, RangeVa, RangeSize - SizePages))
		{
			// roll back our changes
			Node->StartVa = RangeVa;
			Node->Size = RangeSize;
			Status = STATUS_INSUFFICIENT_MEMORY;
			goto Exit;
		}
		
		goto Success;
	}
	
	// We need to cut from both sides.
	
	// Try to create two range items to surround the newly carved range.
	PMMADDRESS_NODE Node1, Node2;
	Node1 = MmAllocatePool(POOL_NONPAGED, Heap->ItemSize);
	Node2 = MmAllocatePool(POOL_NONPAGED, Heap->ItemSize);
	
	if (!Node1 || !Node2)
	{
		// Return out of memory status after freeing them.
		if (Node1) MmFreePool(Node1);
		if (Node2) MmFreePool(Node2);
		
		Status = STATUS_INSUFFICIENT_MEMORY;
		goto Exit;
	}
	
	Node->StartVa = Va;
	Node->Size = SizePages;
	
	// The left node.
	Node1->StartVa = RangeVa;
	Node1->Size = (Va - RangeVa) / PAGE_SIZE;
	
	// The right node.
	Node2->StartVa = Va + SizePages * PAGE_SIZE;
	Node2->Size = (RangeVa + RangeSize * PAGE_SIZE - Node2->StartVa) / PAGE_SIZE;
	
	InsertItemRbTree(&Heap->Tree, &Node1->Entry);
	InsertItemRbTree(&Heap->Tree, &Node2->Entry);
	
	// Now the node is ready to be used.
	
Success:
	RemoveItemRbTree(&Heap->Tree, &Node->Entry);
	*OutNode = Node;
	Status = STATUS_SUCCESS;
	
Exit:
	KeReleaseMutex(&Heap->Mutex);
	return Status;
}

BSTATUS MmAllocateAddressSpace(PMMHEAP Heap, size_t SizePages, bool TopDown, PMMADDRESS_NODE* OutNode)
{
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	PRBTREE_ENTRY Entry;
	
	if (TopDown)
		Entry = GetLastEntryRbTree(&Heap->Tree);
	else
		Entry = GetFirstEntryRbTree(&Heap->Tree);

	// TODO: An optimization *could* be performed here, although it would probably not deliver
	// much performance gain.
	//
	// There could be two entries in the MMADDRESS_NODE, one for the size, and one for the beginning
	// address. The VAD could re-use the size tree entry for a different purpose.
	//
	// The "start address" tree entry should not be repurposed for the "size" tree entry because
	// the heap manager uses it to merge contiguous free segments.

	for (; Entry != NULL; Entry = TopDown ? GetPrevEntryRbTree(Entry) : GetNextEntryRbTree(Entry))
	{
		PMMADDRESS_NODE Node = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		
		if (Node->Size == SizePages)
		{
			// This node fits exactly. Remove it from the tree and return.
			RemoveItemRbTree(&Heap->Tree, &Node->Entry);
			KeReleaseMutex(&Heap->Mutex);
			*OutNode = Node;
			return STATUS_SUCCESS;
		}

		if (Node->Size < SizePages)
		{
			// The size requested does not fit this node. 
			continue;
		}

		// This node is bigger than the requested size.  Split it into two, remove this node
		// from the tree and return it.
		uintptr_t NewNodeVa;
		uintptr_t AllocatedVa;
		size_t NewSize;
		
		if (TopDown)
		{
			AllocatedVa = Node->StartVa + (Node->Size - SizePages) * PAGE_SIZE;
			NewNodeVa = Node->StartVa;
			NewSize = Node->Size - SizePages;
		}
		else
		{
			AllocatedVa = Node->StartVa;
			NewNodeVa = Node->StartVa + SizePages * PAGE_SIZE;
			NewSize = Node->Size - SizePages;
		}
		
		// NOTE: We can do this, because the order of the heap nodes will not actually change.
		uintptr_t OldVa = Node->StartVa;
		Node->StartVa = AllocatedVa;

#ifdef DEBUG
		// However, in debug/check mode, we will actually assert that.
		PMMADDRESS_NODE DebugAddressNode = (PMMADDRESS_NODE) GetNextEntryRbTree(&Node->Entry);
		if (DebugAddressNode)
			ASSERT(DebugAddressNode->StartVa >= Node->StartVa);
#endif

		if (!MmpCreateRegionHeap(Heap, NewNodeVa, NewSize))
		{
			// Well, this didn't work. TODO: Do we just need to refuse?
			ASSERT(!"uh oh");
			
			Node->StartVa = OldVa;
			KeReleaseMutex(&Heap->Mutex);
			return STATUS_INSUFFICIENT_MEMORY;
		}

		Node->Size = SizePages;
		RemoveItemRbTree(&Heap->Tree, &Node->Entry);
		KeReleaseMutex(&Heap->Mutex);
		*OutNode = Node;
		return STATUS_SUCCESS;
	}

	// No big enough ranges exist, so bail.
	KeReleaseMutex(&Heap->Mutex);
	return STATUS_INSUFFICIENT_VA_SPACE;
}

void MmpTryMergeNext(PMMHEAP Heap, PMMADDRESS_NODE Node)
{
	PRBTREE_ENTRY NextEntry = GetNextEntryRbTree(&Node->Entry);
	if (!NextEntry)
		return;

	PMMADDRESS_NODE Next = CONTAINING_RECORD(NextEntry, MMADDRESS_NODE, Entry);
	if (Next->StartVa == Node_EndVa(Node))
	{
		// We can merge.
		RemoveItemRbTree(&Heap->Tree, &Next->Entry);
		Node->Size += Next->Size;
		MmFreePool(Next);
	}
}

BSTATUS MmFreeAddressSpace(PMMHEAP Heap, PMMADDRESS_NODE Node)
{
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);

	// Insert the tree into the heap.
	bool Inserted = InsertItemRbTree(&Heap->Tree, &Node->Entry);

	// If the node couldn't be inserted, this is an error.
	if (!Inserted) {
		KeReleaseMutex(&Heap->Mutex);
		return STATUS_INVALID_PARAMETER;
	}

	// Now, look for the "next" and "previous" entries.  If they are directly adjacent, then merge.
	MmpTryMergeNext(Heap, Node);

	PRBTREE_ENTRY Entry = GetPrevEntryRbTree(&Node->Entry);
	if (Entry)
	{
		Node = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		MmpTryMergeNext(Heap, Node);
	}

	KeReleaseMutex(&Heap->Mutex);
	return STATUS_SUCCESS;
}

void MmDebugDumpHeap()
{
	PMMHEAP Heap = &PsGetAttachedProcess()->Heap;
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&Heap->Tree);
	
	DbgPrint("HeapPtr          Start            End");
	if (!Entry)
		DbgPrint("There are no heap entries.");
	
	while (Entry)
	{
		PMMADDRESS_NODE Node = (PMMADDRESS_NODE) Entry;
		
		DbgPrint("%p %p %p",
			Node,
			Node->StartVa,
			Node->StartVa + Node->Size * PAGE_SIZE
		);
		
		Entry = GetNextEntryRbTree(Entry);
	}
	
	KeReleaseMutex(&Heap->Mutex);
}
