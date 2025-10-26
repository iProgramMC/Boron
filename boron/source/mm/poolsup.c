/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/poolsup.c
	
Abstract:
	This module contains the implementation of the pool
	memory allocator's support routines.
	
Author:
	iProgramInCpp - 24 September 2023
***/
#include "mi.h"

#ifdef IS_32_BIT

// the structure of the pool header PTE if this is set is as follows:
//
// [Bits 31..12] 1 [Bits 11..10] 0 [Bits 9..3] 0
//
// - Bit 8 is cleared because MM_DPTE_COMMITTED shouldn't conflict
//   with this scheme.
//
// - Bit 0 is cleared because MM_PTE_PRESENT conflicts
//
// - Bit 11 is set because that's MM_PTE_ISPOOLHDR

typedef union
{
	MMPTE Pte;
	
	struct
	{
		uintptr_t Present   : 1; // MUST be zero
		uintptr_t B3to9     : 7;
		uintptr_t Committed : 1; // MUST be zero
		uintptr_t B10to11   : 2;
		uintptr_t IsPoolHdr : 1; // MUST be ONE
		uintptr_t B12to31   : 20;
	}
	PACKED;
}
MMPTE_POOLHEADER;

static_assert(sizeof(MMPTE_POOLHEADER) == sizeof(uint32_t));
static_assert(MM_DPTE_COMMITTED == (1 << 8));
static_assert(MM_PTE_ISPOOLHDR == (1 << 11));

MMPTE MiCalculatePoolHeaderPte(uintptr_t Handle)
{
	ASSERT(!(Handle & 0x7));
	MMPTE_POOLHEADER PteHeader;
	PteHeader.Pte = 0;
	
	PteHeader.B3to9 = (Handle >> 3) & 0x7F;
	PteHeader.B10to11 = (Handle >> 10) & 0x3;
	PteHeader.B12to31 = (Handle >> 12);
	PteHeader.IsPoolHdr = true;

	return PteHeader.Pte;
}

FORCE_INLINE
uintptr_t MiReconstructPoolHandleFromPte(MMPTE Pte)
{
	MMPTE_POOLHEADER PteHeader;
	PteHeader.Pte = Pte;
	
	ASSERT(!PteHeader.Present);
	ASSERT(!PteHeader.Committed);
	ASSERT(PteHeader.IsPoolHdr);
	
	return
		PteHeader.B3to9 << 3 |
		PteHeader.B10to11 << 10 |
		PteHeader.B12to31 << 12;
}

#else

#define MiCalculatePoolHeaderPte(Handle) (((uintptr_t)(Handle) - MM_KERNEL_SPACE_BASE) | MM_PTE_ISPOOLHDR)

#define MiReconstructPoolHandleFromPte(Pte) ((MIPOOL_SPACE_HANDLE)(((Pte) & ~MM_PTE_ISPOOLHDR) + MM_KERNEL_SPACE_BASE))

#endif

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

#ifdef TARGET_AMD64

// One PML4 entry can map up to 1<<39 (512GB) of memory.
// Thus, our pool will be 512 GB in size.
#define MI_POOL_LOG2_SIZE (39)

#elif defined TARGET_I386

// There will actually be two arenas of pool space:
// 0x80000000 - 0xC0000000 and 0xD0000000 - 0xF0000000
#define MI_POOL_LOG2_SIZE (30)

#define MI_POOL_LOG2_SIZE_2ND (28)

#else

#error "Define the pool size for your platform!"

#endif

#ifdef IS_32_BIT

void MiInitializeRootPageTable(int Idx)
{
	PMMPTE Pte = (PMMPTE)MI_PML2_LOCATION + Idx;
	MMPFN Pfn = MmAllocatePhysicalPage();
	
	if (Pfn == PFN_INVALID)
		KeCrashBeforeSMPInit("MiCalculatePoolHeaderPte ERROR: Out of memory!");
	
	*Pte = MmPFNToPhysPage(Pfn) | MM_PTE_PRESENT | MM_PTE_READWRITE;
}

void MiInitializePoolPageTables()
{
	int Size1 = 1 << (MI_POOL_LOG2_SIZE - 22);
	int Size2 = 1 << (MI_POOL_LOG2_SIZE_2ND - 22);
	
	for (int i = MI_GLOBAL_AREA_START; i < MI_GLOBAL_AREA_START + Size1; i++)
		MiInitializeRootPageTable(i);
	
	for (int i = MI_GLOBAL_AREA_START_2ND; i < MI_GLOBAL_AREA_START_2ND + Size2; i++)
		MiInitializeRootPageTable(i);
}

#endif

INIT
void MiInitPool()
{
#ifdef IS_32_BIT
	MiInitializePoolPageTables();
#endif
	
	InitializeListHead(&MmpPoolList);
	
	PMIPOOL_ENTRY Entry = MiCreatePoolEntry();
	Entry->Flags = 0;
	Entry->Tag   = MI_EMPTY_TAG;
	Entry->Size  = 1ULL << (MI_POOL_LOG2_SIZE - 12);
	Entry->Address = MiGetTopOfPoolManagedArea();
	InsertTailList(&MmpPoolList, &Entry->ListEntry);

#ifdef TARGET_I386

	// TODO: Will other 32-bit platforms look similar?
	Entry = MiCreatePoolEntry();
	Entry->Flags = 0;
	Entry->Tag   = MI_EMPTY_TAG;
	Entry->Size  = 1ULL << (MI_POOL_LOG2_SIZE_2ND - 12);
	Entry->Address = MiGetTopOfSecondPoolManagedArea();
	InsertTailList(&MmpPoolList, &Entry->ListEntry);
	
#endif
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
	
	if (!NewEntry)
		return 0;
	
	// Link it such that:
	// PoolEntry ====> NewEntry ====> PoolEntry->Flink
	ASSERT(NewEntry);
	ASSERT(PoolEntry);
	ASSERT(PoolEntry->ListEntry.Flink);
	ASSERT(PoolEntry->ListEntry.Blink);
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

MIPOOL_SPACE_HANDLE MiReservePoolSpaceTaggedSub(size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	PLIST_ENTRY CurrentEntry = MIP_START_ITER(&MmpPoolList);
	
	if (OutputAddress)
		*OutputAddress = NULL;
	
	// This is a first-fit allocator.
	
	while (CurrentEntry != &MmpPoolList)
	{
#ifdef DEBUG
		if (CurrentEntry == NULL)
			KeCrash("HUH?!?  CurrentEntry is NULL");
#endif
		
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
		
#ifdef DEBUG
		if (CurrentEntry->Flink == NULL)
			KeCrash("HUH?!?  CurrentEntry->Flink is NULL!  CurrentEntry: %p");
#endif
		
		CurrentEntry = CurrentEntry->Flink;
	}
	
#ifdef DEBUG
	DbgPrint("ERROR: MiReservePoolSpaceTaggedSub ran out of pool space?! (Dude, we have 512 GiB of VM space, what are you doing?!)");
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
		RemoveEntryList(&Flink->ListEntry);
		ASSERT(Entry->ListEntry.Flink != NULL);
		ASSERT(Entry->ListEntry.Blink != NULL);
		ASSERT(Flink->ListEntry.Flink == NULL);
		ASSERT(Flink->ListEntry.Blink == NULL);
		
		MiDeletePoolEntry(Flink);
	}
}

void MiFreePoolSpaceSub(MIPOOL_SPACE_HANDLE Handle)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	
	// Get the handle to the pool entry.
	PMIPOOL_ENTRY Entry = (PMIPOOL_ENTRY) Handle;
	ASSERT(!(Handle & 0x7));
	ASSERT(Handle >= MM_KERNEL_SPACE_BASE);
	ASSERT(MiReconstructPoolHandleFromPte(MiCalculatePoolHeaderPte(Handle)) == Handle);
	
	if (~Entry->Flags & MI_POOL_ENTRY_ALLOCATED)
	{
		KeCrash("MiFreePoolSpace: Returned a free entry");
	}
	
	Entry->Flags &= ~MI_POOL_ENTRY_ALLOCATED;
	Entry->Tag    = MI_EMPTY_TAG;
	
	MmpTryConnectEntryWithItsFlink(Entry);
	if (Entry->ListEntry.Blink != &MmpPoolList)
		MmpTryConnectEntryWithItsFlink(MIP_BLINK(&Entry->ListEntry));
	
	KeReleaseSpinLock(&MmpPoolLock, OldIpl);
}

void MiFreePoolSpace(MIPOOL_SPACE_HANDLE Handle)
{
	uintptr_t Address = ((PMIPOOL_ENTRY) Handle)->Address;
	
	// Acquire the kernel space lock and zero out its PTE.
	MmLockKernelSpaceExclusive();
	
	PMMPTE PtePtr = MmGetPteLocationCheck(Address, false);
	ASSERT(PtePtr);
	ASSERT(*PtePtr == MiCalculatePoolHeaderPte(Handle));
	*PtePtr = 0;
	
	MmUnlockKernelSpace();
	
	// Now actually free that handle.
	MiFreePoolSpaceSub(Handle);
}

MIPOOL_SPACE_HANDLE MiReservePoolSpaceTagged(size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData)
{
	// The actual size of the desired memory region is passed into SizeInPages.  Add 1 to it.
	// The memory region will actually be formed of:
	// [ 1 Page Reserved ] [ SizeInPages Pages Usable ]
	//
	// The reserved page doesn't actually have anything mapped inside.  Instead, its PTE
	// will contain the address of the pool header, minus MM_KERNEL_SPACE_BASE.  Bit 58 will
	// be set, and the present bit will be clear.
	SizeInPages += 1;
	
	void* OutputAddressSub;
	MIPOOL_SPACE_HANDLE Handle = MiReservePoolSpaceTaggedSub(SizeInPages, &OutputAddressSub, Tag, UserData);
	
	if (!Handle)
		return Handle;
	
	ASSERT(Handle >= MM_KERNEL_SPACE_BASE);
	
	// Acquire the kernel space lock and place the address of the handle into the first part of the PTE.
	MmLockKernelSpaceExclusive();
	
	PMMPTE PtePtr = MmGetPteLocationCheck((uintptr_t) OutputAddressSub, true);
	if (!PtePtr)
	{
		// TODO: Handle this in a nicer way.
		DbgPrint("Could not get PTE for output address %p because we ran out of memory?", OutputAddressSub);
		MiFreePoolSpaceSub(Handle);
		
		return (MIPOOL_SPACE_HANDLE) NULL;
	}
	
	ASSERT(*PtePtr == 0);
	ASSERT((Handle & MM_PTE_PRESENT) == 0);
	
	MMPTE Pte = MiCalculatePoolHeaderPte(Handle);
	ASSERT(MiReconstructPoolHandleFromPte(Pte) == Handle);
	
	*PtePtr = Pte;

	MmUnlockKernelSpace();
	
	*OutputAddress = (void*) ((uintptr_t) OutputAddressSub + PAGE_SIZE);
	return Handle;
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
	return (void*) (((PMIPOOL_ENTRY)Handle)->Address + PAGE_SIZE);
}

size_t MiGetSizeFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE Handle)
{
	return (size_t) (((PMIPOOL_ENTRY)Handle)->Size - 1);
}

uintptr_t MiGetUserDataFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE Handle)
{
	return ((PMIPOOL_ENTRY)Handle)->UserData;
}

MIPOOL_SPACE_HANDLE MiGetPoolSpaceHandleFromAddress(void* AddressV)
{
	uintptr_t Address = (uintptr_t) AddressV;
	
	MmLockKernelSpaceExclusive();
	
	PMMPTE PtePtr = MmGetPteLocationCheck(Address - PAGE_SIZE, false);
	if (!PtePtr)
	{
		MmUnlockKernelSpace();
		ASSERT(!"This is supposed to succeed");
		return (MIPOOL_SPACE_HANDLE) NULL;
	}
	
	// N.B.  This kind of relies on the notion that the address doesn't have
	// the valid bit set.
	uintptr_t PAddress = *PtePtr;
	if (~PAddress & MM_PTE_ISPOOLHDR) {
		KeCrash("Trying to access pool space handle from address %p, but its PTE says %p", AddressV, PAddress);
	}
	ASSERT(PAddress & MM_PTE_ISPOOLHDR);
	PAddress = MiReconstructPoolHandleFromPte(PAddress);
	MIPOOL_SPACE_HANDLE Handle = PAddress;
	MmUnlockKernelSpace();
	return Handle;
}
