/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pool.c
	
Abstract:
	This module contains the implementation of the pool
	memory allocator.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include "mi.h"

//
// TODO: This could be improved, however, it's probably OK for now.
//
// Based on NanoShell's heap manager implementation (crt/src/a_mem.c), however,
// it deals in pages, doesn't allocate the actual memory (only the ranges),
// and the headers are separate from the actual memory (they're managed by slab.c).
//

static PMIPOOL_ENTRY MmpPoolFirst, MmpPoolLast;
static TicketLock    MmpPoolLock;

#define MI_EMPTY_TAG MI_TAG("    ")

void MiInitPool()
{
	PMIPOOL_ENTRY Entry = MmpPoolFirst = MmpPoolLast = MiSlabAllocate(sizeof(MIPOOL_ENTRY));
	
	Entry->Flink = NULL;
	Entry->Blink = NULL;
	Entry->Flags = 0;
	Entry->Tag   = MI_EMPTY_TAG;
	Entry->Size  = 1ULL << (MI_POOL_LOG2_SIZE - 12);
	Entry->Address = MiGetTopOfPoolManagedArea();
}

MIPOOL_SPACE_HANDLE MmpSplitEntry(PMIPOOL_ENTRY PoolEntry, size_t SizeInPages, void** OutputAddress, int Tag)
{
	// Basic case: If PoolEntry's size matches SizeInPages
	if (PoolEntry->Size == SizeInPages)
	{
		// Convert the entire entry into an allocated one, and return it.
		PoolEntry->Flags |= MI_POOL_ENTRY_ALLOCATED;
		PoolEntry->Tag    = Tag;
		
		SLogMsg("Pool Entry: %p", PoolEntry);
		
		*OutputAddress = (void*) PoolEntry->Address;
		
		return (MIPOOL_SPACE_HANDLE) PoolEntry;
	}
	
	// This entry manages the area directly after the PoolEntry does.
	PMIPOOL_ENTRY NewEntry = MiSlabAllocate(sizeof(MIPOOL_ENTRY));
	
	// Link it such that:
	// PoolEntry ====> NewEntry ====> PoolEntry->Flink
	NewEntry->Flink = PoolEntry->Flink;
	NewEntry->Blink = PoolEntry;
	if (NewEntry->Blink)
		NewEntry->Blink->Flink = NewEntry;
	if (NewEntry->Flink)
		NewEntry->Flink->Blink = NewEntry;
	
	if (MmpPoolLast == PoolEntry)
		MmpPoolLast  = NewEntry;
	
	// Assign the other properties
	NewEntry->Flags = 0; // Area is free
	NewEntry->Tag   = MI_EMPTY_TAG;
	NewEntry->Size  = PoolEntry->Size - SizeInPages;
	NewEntry->Address = PoolEntry->Address + SizeInPages * PAGE_SIZE;
	
	// Update the properties of the pool entry
	PoolEntry->Size   = SizeInPages;
	PoolEntry->Tag    = Tag;
	PoolEntry->Flags |= MI_POOL_ENTRY_ALLOCATED;
	
	SLogMsg("PoolEntry: %p  NewEntry: %p", PoolEntry, NewEntry);
	
	// Update the output address
	*OutputAddress = (void*) PoolEntry->Address;
	
	return (MIPOOL_SPACE_HANDLE) PoolEntry;
}

MIPOOL_SPACE_HANDLE MiReservePoolSpaceTagged(size_t SizeInPages, void** OutputAddress, int Tag)
{
	KeLockTicket(&MmpPoolLock);
	PMIPOOL_ENTRY Current = MmpPoolFirst;
	
	*OutputAddress = NULL;
	
	// This is a first-fit allocator.
	
	while (Current)
	{
		// Skip allocated entries.
		if (Current->Flags & MI_POOL_ENTRY_ALLOCATED)
		{
			Current = Current->Flink;
			continue;
		}
		
		if (Current->Size >= SizeInPages)
		{
			MIPOOL_SPACE_HANDLE Handle = MmpSplitEntry(Current, SizeInPages, OutputAddress, Tag);
			KeUnlockTicket(&MmpPoolLock);
			return Handle;
		}
		
		Current = Current->Flink;
	}
	
#ifdef DEBUG
	SLogMsg("ERROR: MiReservePoolSpaceTagged ran out of pool space?! (Dude, we have 512 GiB of VM space, what are you doing?!)");
#endif
	
	*OutputAddress = NULL;
	Current = Current->Flink;
	KeUnlockTicket(&MmpPoolLock);
	return (MIPOOL_SPACE_HANDLE) NULL;
}

static void MmpTryConnectEntryWithItsFlink(PMIPOOL_ENTRY Entry)
{
	if (!Entry)
		return;
	
	PMIPOOL_ENTRY Flink = Entry->Flink;
	if (Flink &&
		~Flink->Flags & MI_POOL_ENTRY_ALLOCATED &&
		Flink->Address == Entry->Address + Entry->Size * PAGE_SIZE)
	{
		Entry->Size += Flink->Size;
		Entry->Flink = Flink->Flink;
		
		if (Entry->Flink)
			Entry->Flink->Blink = Entry;
		
		if (MmpPoolLast == Flink)
			MmpPoolLast  = Entry;
		
		MiSlabFree(Flink, sizeof(MIPOOL_ENTRY));
	}
}

void MiFreePoolSpace(MIPOOL_SPACE_HANDLE Handle)
{
	KeLockTicket(&MmpPoolLock);
	
	// Get the handle to the pool entry.
	PMIPOOL_ENTRY Entry = (PMIPOOL_ENTRY) Handle;
	
	if (~Entry->Flags & MI_POOL_ENTRY_ALLOCATED)
	{
		SLogMsg("AAAA");
		KeCrash("MiFreePoolSpace: Returned a free entry");
	}
	
	Entry->Flags &= ~MI_POOL_ENTRY_ALLOCATED;
	Entry->Tag    = MI_EMPTY_TAG;
	
	MmpTryConnectEntryWithItsFlink(Entry);
	MmpTryConnectEntryWithItsFlink(Entry->Blink);
	
	KeUnlockTicket(&MmpPoolLock);
}

void MiDumpPoolInfo()
{
#ifdef DEBUG
	KeLockTicket(&MmpPoolLock);
	PMIPOOL_ENTRY Current = MmpPoolFirst;
	
	SLogMsg("MiDumpPoolInfo:");
	SLogMsg("  EntryAddr         State   Tag     BaseAddr         Limit              SizePages");
	
	while (Current)
	{
		char Tag[8];
		Tag[4] = 0;
		*((int*)Tag) = Current->Tag;
		
		const char* UsedText = "Free";
		if (Current->Flags & MI_POOL_ENTRY_ALLOCATED)
			UsedText = "Used";
		
		SLogMsg("* %p  %s    %s    %p %p   %18zu",
			Current,
			UsedText,
			Tag,
			Current->Address,
			Current->Address + Current->Size * PAGE_SIZE,
			Current->Size);
			
		Current = Current->Flink;
	}
	
	SLogMsg("MiDumpPoolInfo done");
	KeUnlockTicket(&MmpPoolLock);
#endif
}
