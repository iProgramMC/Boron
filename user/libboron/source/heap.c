/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	borondll/src/heap.c
	
Abstract:
	This module implements the OSDLL's heap manager.
	
Author:
	iProgramInCpp - 14 July 2025
***/
#include <boron.h>
#include <list.h>
#include <string.h>
#include <rtl/assert.h>

//#define HEAP_DEBUG

#ifdef HEAP_DEBUG
#define HeapDbg(...) DbgPrint("[HeapDbg] " __VA_ARGS__)
#else
#define HeapDbg(...)
#endif

#ifdef _WIN32

#define ABORT() abort()

#else

#define ABORT() RtlAbort()

#endif

// If an allocation would leave less than this many bytes available (including header),
// then don't actually split the entry into two.
#define MIN_FREE_GAP       (16)

// The default size of an arena of memory.
#define DEFAULT_ARENA_SIZE (128 * 1024)

typedef struct
{
	LIST_ENTRY BlockListEntry;

	// This space can be re-used when this block is allocated.
	union
	{
		LIST_ENTRY FreeListEntry;

		struct
		{
			unsigned WasMapped : 1;
			unsigned Padding : 31;
		};
	};

#ifdef IS_64_BIT
	struct
	{
		size_t BlockSize : 62;
		unsigned IsFree : 1;
		unsigned IsArenaStart : 1; // Must only be used for actual arenas.
	};
#else
	size_t BlockSize;
	struct
	{
		unsigned IsFree : 1;
		unsigned IsArenaStart : 1; // Must only be used for actual arenas.
	};
#endif

	char Data[];
}
OS_HEAP_HEADER, *POS_HEAP_HEADER;

static void OSAddRegionHeap(POS_HEAP Heap, void* Address, size_t Size)
{
	OS_HEAP_HEADER* Header = (OS_HEAP_HEADER*)Address;
	Header->BlockSize = Size - sizeof(OS_HEAP_HEADER);
	Header->IsFree = true;
	
	InsertHeadList(&Heap->BlockList, &Header->BlockListEntry);
	InsertHeadList(&Heap->FreeList,  &Header->FreeListEntry);
}

static POS_HEAP_HEADER OSCreateMappedRegionHeap(POS_HEAP Heap, size_t Size, bool IsArenaStart)
{
	HeapDbg("VirtualAlloc of size %zu...", Size);
	
#ifdef _WIN32

	void* Address = VirtualAlloc(NULL, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!Address)
	{
		DbgPrint("OS: Cannot allocate more memory: %d.", GetLastError());
		return NULL;
	}

#else

	void* Address = NULL;
	size_t RegionSize = Size;
	BSTATUS Status = OSAllocateVirtualMemory(
		CURRENT_PROCESS_HANDLE,
		&Address,
		&RegionSize,
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READ | PAGE_WRITE
	);
	
	if (FAILED(Status))
	{
		DbgPrint("OS: Cannot allocate more memory: %d (%s).", Status, RtlGetStatusString(Status));
		return NULL;
	}

#endif

	HeapDbg("VirtualAlloc of size %zu returned %p.", Size, Address);

	OSAddRegionHeap(Heap, Address, Size);

	OS_HEAP_HEADER* Header = (OS_HEAP_HEADER*)Address;
	Header->IsArenaStart = IsArenaStart;

	return Header;
}

static POS_HEAP_HEADER OSCreateArenaHeap(POS_HEAP Heap)
{
	return OSCreateMappedRegionHeap(Heap, DEFAULT_ARENA_SIZE, true);
}

void* OSAllocateHeap(POS_HEAP Heap, size_t Size)
{
	if (!Size)
		return NULL;
	
	OSEnterCriticalSection(&Heap->CriticalSection);
	
	PLIST_ENTRY Entry;

	// Make sure the size is aligned to 8 bytes.
	Size = (Size + 7) & ~7;

	// Search the free list and return the region.
	while (true)
	{
		Entry = Heap->FreeList.Flink;

		while (Size < DEFAULT_ARENA_SIZE - sizeof(OS_HEAP_HEADER) && Entry != &Heap->FreeList)
		{
			POS_HEAP_HEADER Header = CONTAINING_RECORD(Entry, OS_HEAP_HEADER, FreeListEntry);

			if (Header->BlockSize < Size)
			{
				// No fit.
				Entry = Entry->Flink;
				continue;
			}

			// Check if the fit is within our tolerance.
			if (Header->BlockSize >= Size + MIN_FREE_GAP + sizeof(OS_HEAP_HEADER))
			{
				// No, this is a good fit with some clearance, so setup a new header.
				POS_HEAP_HEADER NewHeader = (POS_HEAP_HEADER)&Header->Data[Size];
				memset(NewHeader, 0, sizeof(OS_HEAP_HEADER));

				// Add this new header to the two lists.
				InsertHeadList(&Header->BlockListEntry, &NewHeader->BlockListEntry);
				InsertHeadList(Entry, &NewHeader->FreeListEntry);

				NewHeader->BlockSize = Header->BlockSize - sizeof(OS_HEAP_HEADER) - Size;
				NewHeader->IsFree = true;
				Header->BlockSize = Size;
			}

			// Now return the block.
			RemoveEntryList(Entry);

			Header->IsFree = false;
			Header->WasMapped = false;
			
			OSLeaveCriticalSection(&Heap->CriticalSection);
			return Header->Data;
		}

		// There are no free regions, or the size is too big.
		// If the size requested is smaller than an arena size, then create one arena
		// and retry.

		if (Size < DEFAULT_ARENA_SIZE - sizeof(OS_HEAP_HEADER))
		{
			POS_HEAP_HEADER Header = OSCreateArenaHeap(Heap);
			if (!Header)
			{
				// Out of virtual address space (or memory)
				OSLeaveCriticalSection(&Heap->CriticalSection);
				return NULL;
			}

			// The arena has been added, so do a retry.
			continue;
		}

		POS_HEAP_HEADER Header = OSCreateMappedRegionHeap(Heap, Size + sizeof(OS_HEAP_HEADER), false);

		RemoveEntryList(&Header->FreeListEntry);

		Header->IsFree = false;
		Header->WasMapped = true;

		OSLeaveCriticalSection(&Heap->CriticalSection);
		return Header->Data;
	}
}

static bool OSTryMergeNext(POS_HEAP Heap, POS_HEAP_HEADER Header)
{
	if (!Header->IsFree)
		return false;
	
	if (Header->BlockListEntry.Flink == &Heap->BlockList)
		return false;

	// This exists, see if it's virtually contiguous with us.
	POS_HEAP_HEADER OtherHeader = CONTAINING_RECORD(Header->BlockListEntry.Flink, OS_HEAP_HEADER, BlockListEntry);

	if ((uintptr_t) &Header->Data[Header->BlockSize] != (uintptr_t) OtherHeader)
		return false;

	// Note: We can't merge contiguous arenas together, because neither Boron nor
	// Win32 support unmapping multiple chunks of memory with one system call.
	if (!OtherHeader->IsFree || OtherHeader->WasMapped || OtherHeader->IsArenaStart)
		return false;

	Header->BlockSize += sizeof(OS_HEAP_HEADER) + OtherHeader->BlockSize;

	RemoveEntryList(&OtherHeader->BlockListEntry);
	RemoveEntryList(&OtherHeader->FreeListEntry);

	return true;
}

#ifdef HEAP_DEBUG

static void OSDebugDumpHeap(POS_HEAP Heap, bool ShowFreeList)
{
	PLIST_ENTRY Entry, Head;

	if (ShowFreeList)
		Head = &Heap->FreeList;
	else
		Head = &Heap->BlockList;

	HeapDbg("\tDumping %s list state for debug:", ShowFreeList ? "Free" : "Block");

	PLIST_ENTRY LastAddress = 0;

	Entry = Head->Flink;
	while (Entry != Head)
	{
		UNUSED
		POS_HEAP_HEADER Hdr;
		if (ShowFreeList)
			Hdr = CONTAINING_RECORD(Entry, OS_HEAP_HEADER, FreeListEntry);
		else
			Hdr = CONTAINING_RECORD(Entry, OS_HEAP_HEADER, BlockListEntry);

		HeapDbg("\t\t%p BSize:%08zu Free:%d ArenaStart:%d Mapped:%d", Hdr, Hdr->BlockSize, Hdr->IsFree, Hdr->IsArenaStart, Hdr->WasMapped);

		// The block list must be ordered by address.
		if (!ShowFreeList)
		{
			ASSERT(LastAddress < Entry);
			LastAddress = Entry;
		}

		Entry = Entry->Flink;
	}
}

#else
	
#define OSDebugDumpHeap(...)

#endif

void OSFreeHeap(POS_HEAP Heap, void* Memory)
{
	if (!Memory)
		return;
	
	OSEnterCriticalSection(&Heap->CriticalSection);
	
	// Access the header.
	POS_HEAP_HEADER Header = CONTAINING_RECORD(Memory, OS_HEAP_HEADER, Data);

	// I know this is not particularly effective, so I might add
	// some signature checks at some point.
	if (Header->IsFree)
	{
		OSLeaveCriticalSection(&Heap->CriticalSection);
		DbgPrint("OS: double-free detected");
		ABORT();
		return;
	}

	// Check if this is an mmapped region.  If yes, then we need to call into the OS to free it.
	if (Header->WasMapped)
	{
		RemoveEntryList(&Header->BlockListEntry);
		RemoveEntryList(&Header->FreeListEntry);

		HeapDbg("Unmapping large region %p", Memory);

#ifdef WIN32

		BOOL success = VirtualFree(Header, 0, MEM_RELEASE);
		if (!success)
			DbgPrint("OS: Cannot free pointer %p: %d.", Header, GetLastError());

#else

		BSTATUS Status = OSFreeVirtualMemory(
			CURRENT_PROCESS_HANDLE,
			Header,
			sizeof(OS_HEAP_HEADER) + Header->BlockSize,
			MEM_RELEASE
		);
		
		if (FAILED(Status))
			DbgPrint("OS: Cannot free pointer %p: %d (%s).", Header, Status, RtlGetStatusString(Status));

#endif

		OSLeaveCriticalSection(&Heap->CriticalSection);
		return;
	}

	// First, mark as free.
	Header->IsFree = true;
	InsertTailList(&Heap->FreeList, &Header->FreeListEntry);

	// If the next item exists, then merge with it.
	OSTryMergeNext(Heap, Header);

	if (Header->BlockListEntry.Blink != &Heap->BlockList)
	{
		// If the previous header was able to eat ours, then we aren't an
		// arena beginning, so switch to the previous header
		POS_HEAP_HEADER OtherHeader = CONTAINING_RECORD(
			Header->BlockListEntry.Blink,
			OS_HEAP_HEADER,
			BlockListEntry
		);

		if (OSTryMergeNext(Heap, OtherHeader))
			Header = OtherHeader;
	}

	// If this is an arena beginning, and its size fills the entirety of the
	// mapped size, then unmap the whole region.
	if (Header->IsArenaStart &&
		Header->BlockSize + sizeof(OS_HEAP_HEADER) == DEFAULT_ARENA_SIZE)
	{
		RemoveEntryList(&Header->BlockListEntry);
		RemoveEntryList(&Header->FreeListEntry);

		HeapDbg("Unmapping arena %p", Header);

#ifdef WIN32
		BOOL success = VirtualFree(Header, 0, MEM_RELEASE);
		if (!success)
			DbgPrint("OS: Cannot free arena %p: %d.", Header, GetLastError());

#else
		BSTATUS Status = OSFreeVirtualMemory(
			CURRENT_PROCESS_HANDLE,
			Header,
			sizeof(OS_HEAP_HEADER) + Header->BlockSize,
			MEM_RELEASE
		);
		
		if (FAILED(Status))
			DbgPrint("OS: Cannot free arena %p: %d (%s).", Header, Status, RtlGetStatusString(Status));
#endif
	}
	
	OSLeaveCriticalSection(&Heap->CriticalSection);
}

BSTATUS OSInitializeHeap(POS_HEAP Heap)
{
	InitializeListHead(&Heap->BlockList);
	InitializeListHead(&Heap->FreeList);
	
	return OSInitializeCriticalSection(&Heap->CriticalSection);
}

void OSDeleteHeap(POS_HEAP Heap)
{
	OSDeleteCriticalSection(&Heap->CriticalSection);
	
	// TODO: Free every block of memory
}

// TODO: Add OSReallocateHeap

// Global Heap
static OS_HEAP OSDLLGlobalHeap;
static bool OSDLLInitializedGlobalHeap;

__attribute__((constructor))
void OSDLLInitializeGlobalHeap()
{
	if (OSDLLInitializedGlobalHeap)
		return;
	
	OSDLLInitializedGlobalHeap = true;
	BSTATUS Status = OSInitializeHeap(&OSDLLGlobalHeap);
	if (FAILED(Status))
	{
		DbgPrint("OSDLL: Failed to initialize heap. %d (%s)", Status, RtlGetStatusString(Status));
		ABORT();
	}
}

void* OSAllocate(size_t Size)
{
	return OSAllocateHeap(&OSDLLGlobalHeap, Size);
}

void OSFree(void* Memory)
{
	OSFreeHeap(&OSDLLGlobalHeap, Memory);
}
