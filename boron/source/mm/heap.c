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

BSTATUS MmAllocateAddressSpace(PMMHEAP Heap, size_t SizePages, bool TopDown, PMMADDRESS_NODE* OutNode)
{
	KeWaitForSingleObject(&Heap->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&Heap->Tree);
	
	if (TopDown)
		Entry = GetLastEntryRbTree(&Heap->Tree);

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
	PMMHEAP Heap = &PsGetCurrentProcess()->Heap;
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
