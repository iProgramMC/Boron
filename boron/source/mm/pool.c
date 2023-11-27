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
// and the headers are separate from the actual memory (they're managed by poolhdr.c).
//

static KSPIN_LOCK MmpPoolLock;
static LIST_ENTRY MmpPoolList;

#define MIP_CURRENT(CE) CONTAINING_RECORD((CE), MIPOOL_ENTRY, ListEntry)
#define MIP_FLINK(E) CONTAINING_RECORD((E)->Flink, MIPOOL_ENTRY, ListEntry)
#define MIP_BLINK(E) CONTAINING_RECORD((E)->Blink, MIPOOL_ENTRY, ListEntry)
#define MIP_START_ITER(Lst) ((Lst)->Flink)

#define MI_EMPTY_TAG MI_TAG("    ")

void MiInitPool()
{
	InitializeListHead(&MmpPoolList);
	
	PMIPOOL_ENTRY Entry = MiCreatePoolEntry();
	Entry->Flags = 0;
	Entry->Tag   = MI_EMPTY_TAG;
	Entry->Size  = 1ULL << (MI_POOL_LOG2_SIZE - 12);
	Entry->Address = MiGetTopOfPoolManagedArea();
	InsertTailList(&MmpPoolList, &Entry->ListEntry);
}

MIPOOL_SPACE_HANDLE MmpSplitEntry(PMIPOOL_ENTRY PoolEntry, size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData)
{
	// Basic case: If PoolEntry's size matches SizeInPages
	if (PoolEntry->Size == SizeInPages)
	{
		// Convert the entire entry into an allocated one, and return it.
		PoolEntry->Flags   |= MI_POOL_ENTRY_ALLOCATED;
		PoolEntry->Tag      = Tag;
		PoolEntry->UserData = UserData;
		
		if (OutputAddress)
			*OutputAddress = (void*) PoolEntry->Address;
		
		return (MIPOOL_SPACE_HANDLE) PoolEntry;
	}
	
	// This entry manages the area directly after the PoolEntry does.
	PMIPOOL_ENTRY NewEntry = MiCreatePoolEntry();
	
	// Link it such that:
	// PoolEntry ====> NewEntry ====> PoolEntry->Flink
	InsertHeadList(&PoolEntry->ListEntry, &NewEntry->ListEntry);
	
	// Assign the other properties
	NewEntry->Flags = 0; // Area is free
	NewEntry->Tag   = MI_EMPTY_TAG;
	NewEntry->Size  = PoolEntry->Size - SizeInPages;
	NewEntry->Address = PoolEntry->Address + SizeInPages * PAGE_SIZE;
	
	// Update the properties of the pool entry
	PoolEntry->Size     = SizeInPages;
	PoolEntry->Tag      = Tag;
	PoolEntry->Flags   |= MI_POOL_ENTRY_ALLOCATED;
	PoolEntry->UserData = UserData;
	
	// Update the output address
	if (OutputAddress)
		*OutputAddress = (void*) PoolEntry->Address;
	
	return (MIPOOL_SPACE_HANDLE) PoolEntry;
}

MIPOOL_SPACE_HANDLE MiReservePoolSpaceTagged(size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	PLIST_ENTRY CurrentEntry = MIP_START_ITER(&MmpPoolList);
	
	if (OutputAddress)
		*OutputAddress = NULL;
	
	// This is a first-fit allocator.
	
	while (CurrentEntry != &MmpPoolList)
	{
		// Skip allocated entries.
		PMIPOOL_ENTRY Current = MIP_CURRENT(CurrentEntry);
		
		if (Current->Flags & MI_POOL_ENTRY_ALLOCATED)
		{
			CurrentEntry = CurrentEntry->Flink;
			continue;
		}
		
		if (Current->Size >= SizeInPages)
		{
			MIPOOL_SPACE_HANDLE Handle = MmpSplitEntry(Current, SizeInPages, OutputAddress, Tag, UserData);
			KeReleaseSpinLock(&MmpPoolLock, OldIpl);
			return Handle;
		}
		
		CurrentEntry = CurrentEntry->Flink;
	}
	
#ifdef DEBUG
	DbgPrint("ERROR: MiReservePoolSpaceTagged ran out of pool space?! (Dude, we have 512 GiB of VM space, what are you doing?!)");
#endif
	
	if (OutputAddress)
		*OutputAddress = NULL;
	
	KeReleaseSpinLock(&MmpPoolLock, OldIpl);
	return (MIPOOL_SPACE_HANDLE) NULL;
}

static void MmpTryConnectEntryWithItsFlink(PMIPOOL_ENTRY Entry)
{
	if (!Entry)
		return;
	
	PMIPOOL_ENTRY Flink = MIP_FLINK(&Entry->ListEntry);
	if (Flink &&
		~Flink->Flags & MI_POOL_ENTRY_ALLOCATED &&
		~Entry->Flags & MI_POOL_ENTRY_ALLOCATED &&
		Flink->Address == Entry->Address + Entry->Size * PAGE_SIZE)
	{
		Entry->Size += Flink->Size;
		
		// remove the 'flink' entry
		RemoveHeadList(&Entry->ListEntry);
		
		MiDeletePoolEntry(Flink);
	}
}

void MiFreePoolSpace(MIPOOL_SPACE_HANDLE Handle)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	
	// Get the handle to the pool entry.
	PMIPOOL_ENTRY Entry = (PMIPOOL_ENTRY) Handle;
	
	if (~Entry->Flags & MI_POOL_ENTRY_ALLOCATED)
	{
		KeCrash("MiFreePoolSpace: Returned a free entry");
	}
	
	Entry->Flags &= ~MI_POOL_ENTRY_ALLOCATED;
	Entry->Tag    = MI_EMPTY_TAG;
	
	MmpTryConnectEntryWithItsFlink(Entry);
	MmpTryConnectEntryWithItsFlink(MIP_BLINK(&Entry->ListEntry));
	
	KeReleaseSpinLock(&MmpPoolLock, OldIpl);
}

void MiDumpPoolInfo()
{
#ifdef DEBUG
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	PLIST_ENTRY CurrentEntry = MIP_START_ITER(&MmpPoolList);
	
	DbgPrint("MiDumpPoolInfo:");
	DbgPrint("  EntryAddr         State   Tag     BaseAddr         Limit              SizePages");
	
	while (CurrentEntry != &MmpPoolList)
	{
		PMIPOOL_ENTRY Current = MIP_CURRENT(CurrentEntry);
		
		char Tag[8];
		Tag[4] = 0;
		*((int*)Tag) = Current->Tag;
		
		const char* UsedText = "Free";
		if (Current->Flags & MI_POOL_ENTRY_ALLOCATED)
			UsedText = "Used";
		
		DbgPrint("* %p  %s    %s    %p %p   %18zu",
			Current,
			UsedText,
			Tag,
			Current->Address,
			Current->Address + Current->Size * PAGE_SIZE,
			Current->Size);
			
		CurrentEntry = CurrentEntry->Flink;
	}
	
	DbgPrint("MiDumpPoolInfo done");
	KeReleaseSpinLock(&MmpPoolLock, OldIpl);
#endif
}

void* MiGetAddressFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE Handle)
{
	return (void*) ((PMIPOOL_ENTRY)Handle)->Address;
}

size_t MiGetSizeFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE Handle)
{
	return (size_t) ((PMIPOOL_ENTRY)Handle)->Size;
}

uintptr_t MiGetUserDataFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE Handle)
{
	return ((PMIPOOL_ENTRY)Handle)->UserData;
}
