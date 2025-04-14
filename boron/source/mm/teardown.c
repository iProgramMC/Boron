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
		
		// MiReleaseVad requires the VAD list lock be held, and releases the lock.
		// We don't ACTUALLY need the lock because the process is gone, over, it has
		// no more threads, and we're the only thread with access to it.
		
		// NOTE: We HAVE to call MmDeCommitVirtualMemory, because it, well, decommits
		// the virtual memory, and *then* we also have to call MiReleaseVad, because
		// this frees the upper page table levels.
		MmLockVadList();
		MiReleaseVad(Vad);
		
		Entry = GetRootEntryRbTree(&Process->VadList.Tree);
	}
	while (Entry);
	
	// Free every heap item.
	Entry = GetRootEntryRbTree(&Process->Heap.Tree);
	do
	{
		
	}
	while (GetRootEntryRbTree(&Process->Heap.Tree));
	
	if (Process->Pcb.PageMap != 0)
		MmFreePhysicalPage(MmPhysPageToPFN(Process->Pcb.PageMap));
	
	PsDetachFromProcess();
}
