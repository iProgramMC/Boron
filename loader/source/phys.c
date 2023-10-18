/*************************************************************\
*                 The Boron Operating System                  *
*                       System Loader                         *
*                                                             *
*              Copyright (C) 2023 iProgramInCpp               *
*                                                             *
*                           phys.c                            *
\*************************************************************/
#include <loader.h>
#include "phys.h"
#include "requests.h"

#ifdef TARGET_AMD64
#include "amd64/mapint.h"
#else
#error Your platform here
#endif

typedef struct limine_memmap_entry MEMMAP_ENTRY, *PMEMMAP_ENTRY;

 MEMMAP_ENTRY MemMapEntryReserve[8192];
PMEMMAP_ENTRY MemMapEntryArray  [8192];
int MemMapEntryCount;
int MemMapLastEntryAdded = -1;
bool MemMapWasCommitted;

// Allocates a continuous memory area. Returns a physical address.
uintptr_t AllocatePages(size_t SizePages, bool IsReservedForKernel)
{
	if (MemMapWasCommitted)
		Crash("Memory map was committed, you can't allocate pages anymore");
	
	uint64_t Type = LIMINE_MEMMAP_BORONLDR_RECLAIMABLE;
	
	// Reserved flag for the kernel itself
	if (IsReservedForKernel)
		Type = LIMINE_MEMMAP_KERNEL_AND_MODULES;
	
	for (int i = 0; i < MemMapEntryCount; i++)
	{
		PMEMMAP_ENTRY Entry = &MemMapEntryReserve[i];
		if (Entry->length < SizePages)
			continue;
		
		if (Entry->type != LIMINE_MEMMAP_USABLE)
			continue;
		
		if (Entry->length == SizePages)
		{
			//simple case where the entire entry becomes bootloader reclaimable
			Entry->type = Type;
			return Entry->base;
		}
		
		if (MemMapLastEntryAdded != -1)
		{
			// Check if the last entry that we added continues along with this memory region.
			// Usually the case.
			PMEMMAP_ENTRY LastEntry = &MemMapEntryReserve[MemMapLastEntryAdded];
			
			// [LastEntry][Entry] case
			if (LastEntry->type == Type && LastEntry->base + LastEntry->length * PAGE_SIZE == Entry->base)
			{
				uint64_t OldBase = Entry->base;
				
				// Increase length of LastEntry
				LastEntry->length += SizePages;
				// Decrease length of Entry and increase base
				Entry->length -= SizePages;
				Entry->base   += SizePages * PAGE_SIZE;
				
				return OldBase;
			}
		}
		
		// Add a new memmap entry.
		if (MemMapLastEntryAdded == (int) ARRAY_COUNT(MemMapEntryReserve))
			Crash("MemMap exhausted trying to allocate memory");
		
		MemMapLastEntryAdded = MemMapEntryCount;
		PMEMMAP_ENTRY NewEntry = &MemMapEntryReserve[MemMapEntryCount++];
		NewEntry->base = Entry->base;
		NewEntry->type = Type;
		NewEntry->length = SizePages;
		
		Entry->base   += SizePages * PAGE_SIZE;
		Entry->length -= SizePages;
		
		return NewEntry->base;
	}
	
	Crash("Out of memory");
	return 0;
}

void DumpMemMap()
{
	for (int i = 0; i < MemMapEntryCount; i++)
	{
		PMEMMAP_ENTRY Entry = &MemMapEntryReserve[i];
		
		LogMsg("entry base %p, length %9lld pages, type %lld", Entry->base, Entry->length, Entry->type);
	}
}

void InitializePmm()
{
	MemMapWasCommitted = false;
	
	struct limine_memmap_response *MemMap = MemMapRequest.response;
	size_t EntryCount = MemMap->entry_count;
	
	if (EntryCount > 8192)
		Crash("Memmap contains more than 8192 entries.");
	
	for (size_t i = 0; i < EntryCount; i++)
	{
		PMEMMAP_ENTRY Entry = MemMap->entries[i];
		
		MemMapEntryReserve[i] = *Entry;
		
		// for simplicity. When committing, we'll expand it again
		MemMapEntryReserve[i].length /= PAGE_SIZE;
	}
	
	MemMap->entries  = MemMapEntryArray;
	MemMapEntryCount = (int)MemMap->entry_count;
}

void CommitMemoryMap()
{
	MemMapWasCommitted = true;
	
	// Commit the memory map to the request. The Boron kernel will use the finalized memory map.
	for (int i = 0; i < MemMapEntryCount; i++)
	{
		MemMapEntryArray[i] = &MemMapEntryReserve[i];
		MemMapEntryArray[i]->length *= PAGE_SIZE;
	}
	
	struct limine_memmap_response *MemMap = MemMapRequest.response;
	MemMap->entry_count = MemMapEntryCount;
	MemMap->entries = MemMapEntryArray;
}
