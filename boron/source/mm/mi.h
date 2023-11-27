/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/mi.h
	
Abstract:
	This is the internal header file for the Memory Manager.
	
Author:
	iProgramInCpp - 12 September 2023
***/

#ifndef NS64_MI_H
#define NS64_MI_H

#include <mm.h>
#include <string.h>
#include <hal.h>
#include <ke.h>
#include <_limine.h>

// ===== Slab Allocator =====

struct MISLAB_CONTAINER_tag;

#define MI_SLAB_ITEM_CHECK (0x424C534E4F524F42) // "BORONSLB"

typedef struct MISLAB_ITEM_tag
{
	char     Data[4096 - 72];
	
	uint64_t Bitmap[4]; // Supports down to 16 byte sized items
	
	LIST_ENTRY ListEntry;
	struct MISLAB_CONTAINER_tag *Parent;
	
	BIG_MEMORY_HANDLE MemHandle;
	
	uint64_t Check;
}
MISLAB_ITEM, *PMISLAB_ITEM;

typedef struct MISLAB_CONTAINER_tag
{
	int          ItemSize;
	bool         NonPaged;
	KSPIN_LOCK   Lock;
	LIST_ENTRY   ListHead;
}
MISLAB_CONTAINER, *PMISLAB_CONTAINER;

typedef enum MISLAB_SIZE_tag
{
	MISLAB_SIZE_16,
	MISLAB_SIZE_32,
	MISLAB_SIZE_64,
	MISLAB_SIZE_128,
	MISLAB_SIZE_256,
	MISLAB_SIZE_512,
	MISLAB_SIZE_1024,
	MISLAB_SIZE_COUNT,
}
MISLAB_SIZE;

static_assert(sizeof(MISLAB_ITEM) <= PAGE_SIZE, "This structure needs to fit inside one page.");

void* MiSlabAllocate(bool NonPaged, size_t Size);
void  MiSlabFree(void* Pointer);
void  MiInitSlabs();
int   MmGetSmallestPO2ThatFitsSize(size_t Size);

// ===== Pool Allocator =====

#ifdef TARGET_AMD64

// One PML4 entry can map up to 1<<39 (512GB) of memory.
// Thus, our pool will be 512 GB in size.
#define MI_POOL_LOG2_SIZE (39)

#else

#error "Define the pool size for your platform!"

#endif

typedef struct MIPOOL_ENTRY_tag
{
	LIST_ENTRY ListEntry;                   // Qword 0, 1
	int        Flags;                       // Qword 2
	int        Tag;
	uintptr_t  UserData;                    // Qword 3
	uintptr_t  Address;                     // Qword 4
	size_t     Size;                        // Qword 5, size is in pages.
}
MIPOOL_ENTRY, *PMIPOOL_ENTRY;

typedef enum MIPOOL_ENTRY_FLAGS_tag
{
	MI_POOL_ENTRY_ALLOCATED = (1 << 0),
}
MIPOOL_ENTRY_FLAGS;

// Four character tag.
#define MI_TAG(TagStr) (*((int*)TagStr))

// A handle to an address range managed by the pool manager.
typedef uintptr_t MIPOOL_SPACE_HANDLE;

// Reserve space in the kernel pool.
// The return value is an *opaque* value that must be passed as is to MiFreePoolSpace.
// The tag is a value used to identify and track memory usage.

// [!!!] The output address is NOT mapped in, and in fact, it is guaranteed that no PTE
//       is valid at that address. The allocator will have to map their own memory there
//       and free it when they are done.
MIPOOL_SPACE_HANDLE MiReservePoolSpaceTagged(size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData);

// Free an address space in the kernel pool.

// [!!!] This DOES NOT free the page entries mapped into the space - it merely returns
//       the _address range_ to the system.

void MiFreePoolSpace(MIPOOL_SPACE_HANDLE);

// Initializes the pool manager.  This must be done on the bootstrap processor.
void MiInitPool();

// Get the memory address from a handle.
void* MiGetAddressFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE);

// Get the size of the address range owned by a handle.
size_t MiGetSizeFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE);

// Get the user data from the address range owned by a handle as passed to MiReservePoolSpaceTagged.
uintptr_t MiGetUserDataFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE);

// Dump info about the pool. Debug only.
void MiDumpPoolInfo();

// ===== Pool entry allocator =====
// Really simple allocator that dishes out pool entries. To get rid of the pool allocator's dependency on the slab allocator.
// The dependency chart will now look like this:
// [PoolEntryAllocator] <----- [PoolAllocator] <----- [SlabAllocator]


PMIPOOL_ENTRY MiCreatePoolEntry();

void MiDeletePoolEntry(PMIPOOL_ENTRY Entry);

// ===== Page table manager =====

// Prepare a pml4 entry for the pool allocator.
void MiPrepareGlobalAreaForPool(HPAGEMAP PageMap);

// Get the top of the area managed by the pool allocator.
uintptr_t MiGetTopOfPoolManagedArea();

#endif//NS64_MI_H