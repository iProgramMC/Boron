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

void MmInitializeHeap(PMMHEAP Heap)
{
	InitializeRbTree(&Heap->Tree);
	KeInitializeMutex(&Heap->Mutex, 0);
}

// Used during initialization.  Don't call otherwise
bool MmpCreateRegionHeap(PMMHEAP Heap, uintptr_t StartVa, size_t Size)
{
	PMMADDRESS_NODE Mem = MmAllocatePool(POOL_NONPAGED, sizeof(MMADDRESS_NODE));
	if (!Mem)
		return false;

	InitializeRbTreeEntry(&Mem->Entry);
	Mem->StartVa = StartVa;
	Mem->Size = Size;

	InsertItemRbTree(&Heap->Tree, &Mem->Entry);
	return true;
}

PMMADDRESS_NODE MmAllocateAddressSpace(PMMHEAP Heap, size_t SizePages)
{
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE);
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&Heap->Tree);

	// TODO: An optimization *could* be performed here, although it would probably not deliver
	// much performance gain.
	//
	// There could be two entries in the MMADDRESS_NODE, one for the size, and one for the beginning
	// address. The VAD could re-use the size tree entry for a different purpose.
	//
	// The "start address" tree entry should not be repurposed for the "size" tree entry because
	// the heap manager uses it to merge contiguous free segments.

	for (; Entry != NULL; Entry = GetNextEntryRbTree(Entry))
	{
		PMMADDRESS_NODE Node = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		
		if (Node->Size == SizePages) {
			// This node fits exactly. Remove it from the tree and return.
			RemoveItemRbTree(&Heap->Tree, &Node->Entry);
			KeReleaseMutex(&Heap->Mutex);
			return Node;
		}

		if (Node->Size < SizePages) {
			// The size requested does not fit this node. 
			continue;
		}

		// This node is bigger than the requested size.  Split it into two, remove this node
		// from the tree and return it.
		uintptr_t NewVa = Node->StartVa + SizePages * PAGE_SIZE;
		size_t NewSize = Node->Size - SizePages;
		if (!MmpCreateRegionHeap(Heap, NewVa, NewSize))
		{
			// Well, this didn't work. TODO
			ASSERT(!"uh oh");
			KeReleaseMutex(&Heap->Mutex);
			return NULL;
		}

		Node->Size = SizePages;
		RemoveItemRbTree(&Heap->Tree, &Node->Entry);
		KeReleaseMutex(&Heap->Mutex);
		return Node;
	}

	// No big enough ranges exist, so bail.
	KeReleaseMutex(&Heap->Mutex);
	return NULL;
}

void MmpTryMergeNext(PMMHEAP Heap, PMMADDRESS_NODE Node)
{
	PRBTREE_ENTRY NextEntry = GetNextEntryRbTree(&Node->Entry);
	if (!NextEntry)
		return;

	PMMADDRESS_NODE Next = CONTAINING_RECORD(NextEntry, MMADDRESS_NODE, Entry);
	if (Next->StartVa == Node_EndVa(Node)) {
		// We can merge.
		RemoveItemRbTree(&Heap->Tree, &Next->Entry);
		Node->Size += Next->Size;
		MmFreePool(Next);
	}
}

bool MmFreeAddressSpace(PMMHEAP Heap, PMMADDRESS_NODE Node)
{
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE);

	// Insert the tree into the heap.
	bool Inserted = InsertItemRbTree(&Heap->Tree, &Node->Entry);

	// If the node couldn't be inserted, this is an error.
	if (!Inserted) {
		KeReleaseMutex(&Heap->Mutex);
		return false;
	}

	// Now, look for the "next" and "previous" entries.  If they are directly adjacent, then merge.
	MmpTryMergeNext(Heap, Node);

	PRBTREE_ENTRY Entry = GetPrevEntryRbTree(&Node->Entry);
	if (Entry) {
		Node = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		MmpTryMergeNext(Heap, Node);
	}

	KeReleaseMutex(&Heap->Mutex);
	return true;
}
