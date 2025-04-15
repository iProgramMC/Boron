/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/teardown.c
	
Abstract:
	This module implements the memory management process
	teardown function.
	
Author:
	iProgramInCpp - 14 April 2025
***/
#include "mi.h"

// Normally you don't call MiReleaseVad or MiDecommitVad directly.
void MiReleaseVad(PMMVAD Vad);
void MiDecommitVad(PMMVAD_LIST VadList, PMMVAD Vad, size_t StartVa, size_t SizePages);

void MmTearDownProcess(PEPROCESS Process)
{
	// This thread will attach to this process to perform teardown on it.
	PsAttachToProcess(Process);
	
	// Free every VAD.
	PRBTREE_ENTRY Entry = GetRootEntryRbTree(&Process->VadList.Tree);
	do
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		
		// MiDecommitVad and MiReleaseVad require the VAD list lock be
		// held, but they release the lock.
		//
		// The thing is, we are the ONLY thread that has the reference
		// to this process.  As such, there is no possibility of data
		// races or anything nasty like that.
		//
		// As such, we can get away with just locking the VAD list twice
		// and having the two internal calls unlock it.
		
		MmLockVadList();
		MiDecommitVad(&Process->VadList, Vad, Vad->Node.StartVa, Vad->Node.Size);
		
		MmLockVadList();
		MiReleaseVad(Vad);
		
		Entry = GetRootEntryRbTree(&Process->VadList.Tree);
	}
	while (Entry);
	
	// Free every heap item.
	Entry = GetRootEntryRbTree(&Process->Heap.Tree);
	do
	{
		PMMADDRESS_NODE Node = CONTAINING_RECORD(Entry, MMADDRESS_NODE, Entry);
		
		// All we need to do is remove the item from the tree and free the item's memory.
		RemoveItemRbTree(&Process->Heap.Tree, &Node->Entry);
		MmFreePool(Node);
		
		Entry = GetRootEntryRbTree(&Process->Heap.Tree);
	}
	while (Entry);
	
#if defined(DEBUG) && defined(TARGET_AMD64)

	PMMPTE PteScan = MmGetHHDMOffsetAddr(Process->Pcb.PageMap);
	for (int i = 0; i < 256; i++)
		ASSERT(~PteScan[i] & MM_PTE_PRESENT);

#endif
	
	if (Process->Pcb.PageMap != 0)
		MmFreePhysicalPage(MmPhysPageToPFN(Process->Pcb.PageMap));
	
	PsDetachFromProcess();
}
