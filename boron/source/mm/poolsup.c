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

#ifndef DEBUG2
#define TestDbgPrint(...)
#else
#define TestDbgPrint DbgPrint
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

static PMIPOOL_ENTRY MmpPoolLazyReleaseList = NULL;
static int MmpPoolLazyReleaseCounter;

#define MIP_CURRENT(CE) CONTAINING_RECORD((CE), MIPOOL_ENTRY, ListEntry)
#define MIP_FLINK(E) CONTAINING_RECORD((E)->Flink, MIPOOL_ENTRY, ListEntry)
#define MIP_BLINK(E) CONTAINING_RECORD((E)->Blink, MIPOOL_ENTRY, ListEntry)
#define MIP_START_ITER(Lst) ((Lst)->Flink)

#define MI_EMPTY_TAG MI_TAG("    ")

#ifdef TARGET_I386

void MiInitializeRootPageTable(int Idx)
{
	PMMPTE Pte = (PMMPTE)MI_PML2_LOCATION + Idx;
	MMPFN Pfn = MmAllocatePhysicalPage();
	
	if (Pfn == PFN_INVALID)
		KeCrashBeforeSMPInit("MiInitializeRootPageTable ERROR: Out of memory!");
	
	*Pte = MmBuildPte(Pfn, MM_PROT_READ | MM_PROT_WRITE | MM_MISC_IS_FROM_PMM);
	MmFlushTlbUpdates();
}

#elif defined TARGET_ARM

#define L1PTE_FLAGS_CPT 0b01

void MiInitializeRootPageTable(int Idx)
{
	PMMPTE Pte = (PMMPTE) MI_PML1_LOCATION;
	MMPFN Pfn = MmAllocatePhysicalPage();
	
	if (Pfn == PFN_INVALID)
		KeCrashBeforeSMPInit("MiInitializeRootPageTable ERROR: Out of memory!");
	
	for (int i = 0; i < 4; i++) {
		// HACK for now.
		MMPTE hPte;
		hPte.PteHardware = ((Pfn << 12) + i * 1024) | L1PTE_FLAGS_CPT;
		Pte[Idx * 4 + i] = hPte;
	}
	
	// Also update Debbie
	Pte = (PMMPTE) MI_PML2_MIRROR_LOCATION;
	Pte[Idx] = MmBuildPte(Pfn, MM_MISC_IS_FROM_PMM | MM_PROT_READ | MM_PROT_WRITE);
}

#endif

#ifdef IS_32_BIT

void MiInitializePoolPageTables()
{
	int Size1 = 1 << (MI_POOL_LOG2_SIZE - 22);
	int Size2 = 1 << (MI_POOL_LOG2_SIZE_2ND - 22);
	
	for (int i = MI_GLOBAL_AREA_START; i < MI_GLOBAL_AREA_START + Size1; i++)
		MiInitializeRootPageTable(i);
	
	for (int i = MI_GLOBAL_AREA_START_2ND; i < MI_GLOBAL_AREA_START_2ND + Size2; i++)
		MiInitializeRootPageTable(i);
	
	MmFlushTlbUpdates();
}

#endif

INIT
void MiInitPool()
{
#ifdef IS_32_BIT
	MiInitializePoolPageTables();
#endif
	
	InitializeListHead(&MmpPoolList);
	MmpPoolLazyReleaseList = NULL;
	
	PMIPOOL_ENTRY Entry = MiCreatePoolEntry();
	Entry->Flags = 0;
	Entry->Tag   = MI_EMPTY_TAG;
	Entry->Size  = 1ULL << (MI_POOL_LOG2_SIZE - 12);
	Entry->Address = MiGetTopOfPoolManagedArea();
	InsertTailList(&MmpPoolList, &Entry->ListEntry);

#ifdef MI_USE_TWO_POOLS

	// TODO: Will other 32-bit platforms look similar?
	Entry = MiCreatePoolEntry();
	Entry->Flags = 0;
	Entry->Tag   = MI_EMPTY_TAG;
	Entry->Size  = 1ULL << (MI_POOL_LOG2_SIZE_2ND - 12);
	Entry->Address = MiGetTopOfSecondPoolManagedArea();
	InsertTailList(&MmpPoolList, &Entry->ListEntry);
	
#endif
}

void MiFreePoolSpaceSubLocked(MIPOOL_SPACE_HANDLE Handle);

// Returns true if regions have been reclaimed, false if there were none to reclaim.
static bool MmpReclaimLazyReleaseList()
{
	ASSERT(MmpPoolLock.Locked);
	
	bool Reclaimed = false;
	
	TestDbgPrint("MmpReclaimLazyReleaseList: Checking for reclaimable regions.");
	while (MmpPoolLazyReleaseList)
	{
		PMIPOOL_ENTRY Entry = MmpPoolLazyReleaseList;
		MmpPoolLazyReleaseList = Entry->NextLazyEntry;
		
		Entry->Flags &= ~(MI_POOL_ENTRY_LAZY_RELEASE | MI_POOL_ENTRY_PENDING_FREE);
		
		TestDbgPrint("	MmpReclaimLazyReleaseList: Freeing region %p (address %p).", Entry, Entry->Address);
		MiFreePoolSpaceSubLocked((MIPOOL_SPACE_HANDLE) Entry);
		Reclaimed = true;
	}
	
	if (Reclaimed)
	{
		// Issue a complete TLB shootdown.
		TestDbgPrint("	MmpReclaimLazyReleaseList: Clearing TLB.");
		MmIssueTLBShootDown(MiGetTopOfPoolManagedArea(), MAX_TLBS_LENGTH, NULL);
	}
	
	MmpPoolLazyReleaseCounter = 0;
	return Reclaimed;
}

MIPOOL_SPACE_HANDLE MmpSplitEntry(PMIPOOL_ENTRY PoolEntry, size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData, int EntryFlags)
{
	// Basic case: If PoolEntry's size matches SizeInPages
	if (PoolEntry->Size == SizeInPages)
	{
		// Convert the entire entry into an allocated one, and return it.
		PoolEntry->Flags    = MI_POOL_ENTRY_ALLOCATED | EntryFlags;
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
	PoolEntry->Flags    = MI_POOL_ENTRY_ALLOCATED | EntryFlags;
	PoolEntry->UserData = UserData;
	
	// Update the output address
	if (OutputAddress)
		*OutputAddress = (void*) PoolEntry->Address;
	
	return (MIPOOL_SPACE_HANDLE) PoolEntry;
}

MIPOOL_SPACE_HANDLE MiReservePoolSpaceTaggedSub(size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData, int EntryFlags)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	PLIST_ENTRY CurrentEntry = MIP_START_ITER(&MmpPoolList);
	
	if (OutputAddress)
		*OutputAddress = NULL;
	
	// The while loop is so that in case there are no pool regions left, 
	while (true)
	{
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
				MIPOOL_SPACE_HANDLE Handle = MmpSplitEntry(Current, SizeInPages, OutputAddress, Tag, UserData, EntryFlags);
				
				if (Handle == 0) {
					// Running out of physical memory, too.
					// Jump directly to reclaiming from the lazy release list.
					DbgPrint("MiReservePoolSpaceTaggedSub: MmpSplitEntry returned zero, reclaiming right away.");
					break;
				}
				
				KeReleaseSpinLock(&MmpPoolLock, OldIpl);
				return Handle;
			}
			
#ifdef DEBUG
			if (CurrentEntry->Flink == NULL)
				KeCrash("HUH?!?  CurrentEntry->Flink is NULL!  CurrentEntry: %p");
#endif
			
			CurrentEntry = CurrentEntry->Flink;
		}
		
		// No more space or memory.  See if we can reclaim the lazy release list.
		if (!MmpReclaimLazyReleaseList())
		{
			// Couldn't reclaim, bail out now!
			break;
		}
	}
	
#ifdef DEBUG
	DbgPrint("ERROR: MiReservePoolSpaceTaggedSub ran out of pool space?! (Dude, we have "
	#ifdef IS_32_BIT
		"1.25 GiB"
	#else
		"512 GiB"
	#endif
		" of VM space, what are you doing?!)");
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

void MiFreePoolSpaceSubLocked(MIPOOL_SPACE_HANDLE Handle)
{
	// Get the handle to the pool entry.
	PMIPOOL_ENTRY Entry = (PMIPOOL_ENTRY) Handle;
	ASSERT(!(Handle & 0x7));
	ASSERT(Handle >= MM_KERNEL_SPACE_BASE);
	ASSERT(MmGetPoolHeaderAddressPte(MmBuildPoolHeaderPte(Handle)) == Handle);
	
	if (~Entry->Flags & MI_POOL_ENTRY_ALLOCATED)
	{
		KeCrash("MiFreePoolSpace: Returned a free entry");
	}
	
	if (Entry->Flags & MI_POOL_ENTRY_LAZY_RELEASE)
	{
		// Lazy release: add it to the list of regions to lazily release
		Entry->Flags |= MI_POOL_ENTRY_PENDING_FREE;
		Entry->NextLazyEntry = MmpPoolLazyReleaseList;
		MmpPoolLazyReleaseList = Entry;
		
		MmpPoolLazyReleaseCounter++;
		if (MmpPoolLazyReleaseCounter >= MI_MAX_LAZY_RELEASE_COUNT)
		{
			UNUSED bool Result = MmpReclaimLazyReleaseList();
			ASSERT(Result && "Umm... But we were supposed to reclaim, weren't we?");
		}
	}
	else
	{
		Entry->Flags &= ~MI_POOL_ENTRY_ALLOCATED;
		Entry->Tag = MI_EMPTY_TAG;
		
		MmpTryConnectEntryWithItsFlink(Entry);
		if (Entry->ListEntry.Blink != &MmpPoolList)
			MmpTryConnectEntryWithItsFlink(MIP_BLINK(&Entry->ListEntry));
	}
}

void MiFreePoolSpaceSub(MIPOOL_SPACE_HANDLE Handle)
{
	KIPL OldIpl;
	KeAcquireSpinLock(&MmpPoolLock, &OldIpl);
	MiFreePoolSpaceSubLocked(Handle);
	KeReleaseSpinLock(&MmpPoolLock, OldIpl);
}

void MiFreePoolSpace(MIPOOL_SPACE_HANDLE Handle)
{
	uintptr_t Address = ((PMIPOOL_ENTRY) Handle)->Address;
	
	// Acquire the kernel space lock and zero out its PTE.
	MmLockKernelSpaceExclusive();
	
	PMMPTE PtePtr = MmGetPteLocationCheck(Address, false);
	ASSERT(PtePtr);
	ASSERT(MmIsEqualPte(*PtePtr, MmBuildPoolHeaderPte(Handle)));
	*PtePtr = MmBuildZeroPte();
	
	MmFlushTlbUpdates();
	MmUnlockKernelSpace();
	
	// Now actually free that handle.
	MiFreePoolSpaceSub(Handle);
}

MIPOOL_SPACE_HANDLE MiReservePoolSpaceTagged(size_t SizeInPages, void** OutputAddress, int Tag, uintptr_t UserData, int EntryFlags)
{
	// The actual size of the desired memory region is passed into SizeInPages.  Add 1 to it.
	// The memory region will actually be formed of:
	// [ 1 Page Reserved ] [ SizeInPages Pages Usable ]
	//
	// The reserved page doesn't actually have anything mapped inside.  Instead, its PTE
	// will contain the address of the pool header, minus MM_KERNEL_SPACE_BASE.  Bit 58 will
	// be set, and the present bit will be clear.
	SizeInPages += 1;
	
	// Only these flags are valid to specify for this function.
	EntryFlags &= (MI_POOL_ENTRY_LAZY_RELEASE);
	
	void* OutputAddressSub;
	MIPOOL_SPACE_HANDLE Handle = MiReservePoolSpaceTaggedSub(SizeInPages, &OutputAddressSub, Tag, UserData, EntryFlags);
	
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
	
	ASSERT(MmIsEqualPte(*PtePtr, MmBuildZeroPte()));
	ASSERT((Handle & 3) == 0);
	
	MMPTE Pte = MmBuildPoolHeaderPte(Handle);
	ASSERT(MmGetPoolHeaderAddressPte(Pte) == Handle);
	
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
		if (Current->Flags & MI_POOL_ENTRY_PENDING_FREE)
			UsedText = "Lazy";
		else if (Current->Flags & MI_POOL_ENTRY_ALLOCATED)
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

void MiSetUserDataFromPoolSpaceHandle(MIPOOL_SPACE_HANDLE Handle, uintptr_t Data)
{
	((PMIPOOL_ENTRY)Handle)->UserData = Data;
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
	MMPTE Pte = *PtePtr;
	if (!MmIsPoolHeaderPte(Pte)) {
		KeCrash("Trying to access pool space handle from address %p, but its PTE says %p", AddressV, Pte.PteHardware);
	}
	ASSERT(MmIsPoolHeaderPte(Pte));
	
	uintptr_t PAddress = MmGetPoolHeaderAddressPte(Pte);
	MIPOOL_SPACE_HANDLE Handle = PAddress;
	MmUnlockKernelSpace();
	return Handle;
}
