/***
	The Boron Operating System
	Copyright (C) 2024-2025 iProgramInCpp

Module name:
	mm/vad.c
	
Abstract:
	Defines functions pertaining to the VAD (Virtual Address
	Descriptor) list of a process.
	
Author:
	iProgramInCpp - 18 August 2024
***/
#include "mi.h"
#include <ex.h>

MMVAD_LIST MiSystemVadList;

PMMVAD_LIST MmLockVadListProcess(PEPROCESS Process)
{
	MiLockVadList(&Process->VadList);
	return &Process->VadList;
}

void MmInitializeVadList(PMMVAD_LIST VadList)
{
	KeInitializeMutex(&VadList->Mutex, MM_VAD_MUTEX_LEVEL);
	InitializeRbTree(&VadList->Tree);
}

void MmInitializeSystemVadList()
{
	// Initialize the kernel's VAD list.
	MmInitializeVadList(&MiSystemVadList);
}

void MmUnlockVadList(PMMVAD_LIST VadList)
{
	KeReleaseMutex(&VadList->Mutex);
}

// Initializes and inserts a VAD.  If a VAD pointer was not provided
// it also allocates one from pool space.
//
// The StartAddress and SizePages are only used if the VAD doesn't exist.
//
// NOTE: This locks the VAD list and unlocks it if UnlockAfter is true.
BSTATUS MiInitializeAndInsertVad(
	PMMVAD_LIST VadList,
	PMMVAD* InOutVad,
	void* StartAddress,
	size_t SizePages,
	int AllocationType,
	int Protection,
	bool UnlockAfter
)
{
	// If a VAD was not provided then this VAD needs to be allocated
	bool Allocated = false;
	PMMVAD Vad = InOutVad ? *InOutVad : NULL;
	
	if (!Vad)
	{
		Vad = MmAllocatePool(POOL_NONPAGED, sizeof(MMVAD));
		if (!Vad)
			return STATUS_INSUFFICIENT_MEMORY;
		
		Vad->Node.StartVa = (uintptr_t) StartAddress;
		Vad->Node.Size = SizePages;
		
		Allocated = true;
	}
	
	MiLockVadList(VadList);
	
	if (!InsertItemRbTree(&VadList->Tree, &Vad->Node.Entry))
	{
		// Which status should we return here?
	#ifdef DEBUG
		KeCrash("MmInitializeAndInsertVad: Failed to insert VAD into VAD list (address %p)", Vad->Node.StartVa);
	#endif
		MmUnlockVadList(VadList);
		
		if (Allocated)
			MmFreePool(Vad);
		
		return STATUS_INSUFFICIENT_VA_SPACE;
	}
	
	// Clear all the fields in the VAD.
	Vad->Flags.LongFlags  = 0;
	Vad->Mapped.Object    = NULL;
	Vad->SectionOffset    = 0;
	Vad->Flags.Cow        =  (AllocationType & MEM_COW) != 0;
	Vad->Flags.Committed  =  (AllocationType & MEM_COMMIT) != 0;
	Vad->Flags.Private    = (~AllocationType & MEM_SHARED) != 0;
	Vad->Flags.Protection = Protection;
	Vad->ViewCacheLruEntry.Flink = NULL;
	Vad->ViewCacheLruEntry.Blink = NULL;
	
	if (UnlockAfter)
		MmUnlockVadList(VadList);
	
	if (InOutVad)
		*InOutVad = Vad;
	
	return STATUS_SUCCESS;
}

// NOTE: This does not unlock the VAD list.
BSTATUS MmReserveVirtualMemoryVad(size_t SizePages, int AllocationType, int Protection, void* StartAddress, PMMVAD* OutVad, PMMVAD_LIST* OutVadList)
{
	PEPROCESS Process = PsGetAttachedProcess();
	PMMADDRESS_NODE AddrNode;
	
	BSTATUS Status;

	if (StartAddress)
		Status = MmAllocateAddressRange(&Process->Heap, (uintptr_t) StartAddress, SizePages, &AddrNode);
	else
		Status = MmAllocateAddressSpace(&Process->Heap, SizePages, AllocationType & MEM_TOP_DOWN, &AddrNode);
	
	if (FAILED(Status))
		return Status;
	
	PMMVAD Vad = (PMMVAD) AddrNode;
	
	Status = MiInitializeAndInsertVad(&Process->VadList, &Vad, NULL, 0, AllocationType, Protection, false);
	
	if (FAILED(Status))
		KeCrash("TODO: Fix this");
	
	*OutVad = Vad;
	*OutVadList = &Process->VadList;
	return Status;
}

// Reserves a range of virtual memory.
BSTATUS MmReserveVirtualMemory(size_t SizePages, void** InOutAddress, int AllocationType, int Protection)
{
	if (Protection & ~(PAGE_READ | PAGE_WRITE | PAGE_EXECUTE))
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & ~(MEM_RESERVE | MEM_COMMIT | MEM_SHARED | MEM_TOP_DOWN))
		return STATUS_INVALID_PARAMETER;
	
	void* StartVa = *InOutAddress;
	
	PMMVAD Vad;
	PMMVAD_LIST VadList;
	BSTATUS Status = MmReserveVirtualMemoryVad(SizePages, AllocationType, Protection, StartVa, &Vad, &VadList);
	if (FAILED(Status))
		return Status;
	
	*InOutAddress = (void*) Vad->Node.StartVa;
	ASSERT(PAGE_ALIGNED(Vad->Node.StartVa));
	MmUnlockVadList(VadList);
	return STATUS_SUCCESS;
}

// Cleans up all of the references to this VAD, including now-stale
// PTEs and the object reference that this VAD holds.
void MiCleanUpVad(PMMVAD Vad)
{
	// Zero out all of the PTEs.
	KIPL Ipl = MmLockSpaceExclusive(Vad->Node.StartVa);
	
	uintptr_t CurrentVa = Vad->Node.StartVa;
	PMMPTE Pte = MmGetPteLocation(CurrentVa);
	for (size_t i = 0; i < Vad->Node.Size; i++)
	{
		if (i == 0 || ((uintptr_t)Pte & 0xFFF) == 0)
		{
			if (!MmCheckPteLocation(CurrentVa, false))
			{
				// Nothing to zero out here.
				size_t OldVa = CurrentVa;
				CurrentVa = (CurrentVa + PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE)) & ~(PAGE_SIZE * PAGE_SIZE / sizeof(MMPTE) - 1);
				Pte = MmGetPteLocation(CurrentVa);
				i += (CurrentVa - OldVa) / PAGE_SIZE;
				continue;
			}
		}
		
		// Assume that the page is NOT present.
		ASSERT(~*Pte & MM_PTE_PRESENT);
		
		*Pte = 0;
		Pte++;
		CurrentVa += PAGE_SIZE;
	}
	
	MiFreeUnusedMappingLevelsInCurrentMap(Vad->Node.StartVa, Vad->Node.Size);
	
	// Issue a TLB shootdown request covering the whole area. 
	MmIssueTLBShootDown(Vad->Node.StartVa, Vad->Node.Size);
	
	MmUnlockSpace(Ipl, Vad->Node.StartVa);
	
	// Remove the reference to the object if needed.
	if (Vad->Mapped.Object)
	{
		ObDereferenceObject(Vad->Mapped.Object);
		Vad->Mapped.Object = NULL;
	}
}

// Releases a range of virtual memory represented by a VAD.
// The range must be entirely decommitted before it is de-reserved.
// The VAD list lock must be held before calling the function.
void MiReleaseVad(PMMVAD Vad)
{
	PEPROCESS Process = PsGetAttachedProcess();

	// Step 1.  Remove the VAD from the VAD list tree.
	RemoveItemRbTree(&Process->VadList.Tree, &Vad->Node.Entry);
	MmUnlockVadList(&Process->VadList);
	
	// Step 2.  Clean up this VAD after it's freed.
	MiCleanUpVad(Vad);
	
	// Step 3. Finally, add the range into the heap/free list.
	BSTATUS Status = MmFreeAddressSpace(&Process->Heap, &Vad->Node);
	if (FAILED(Status))
	{
		// NOTE: This should never happen.
		//
		// Why? Because there's no way for this region to be used if it's
		// not part of either the heap or the VAD list.
		KeCrash("MmDereserveVad: Failed to insert VAD into free-heap (address %p)", Vad->Node.StartVa);
		Status = STATUS_UNIMPLEMENTED;
	}
}

// This releases a range of virtual memory allocated using MmReserveVirtualMemoryVad.
// The address must point to the base of the region.
// Note that the entire range must have been decommitted first.
BSTATUS MmReleaseVirtualMemory(void* Address)
{
	uintptr_t AddressI = (uintptr_t) Address;
	
	PMMVAD_LIST VadList = MmLockVadListProcess(PsGetAttachedProcess());
	
	// Look up the item in the VAD.
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&VadList->Tree, AddressI);
	
	if (!Entry)
	{
		// No entry found.
		MmUnlockVadList(VadList);
		return STATUS_MEMORY_NOT_RESERVED;
	}
	
	PMMVAD Vad = (PMMVAD) Entry;
	if (Vad->Node.StartVa != AddressI)
	{
		// The virtual address of the VAD does not match the address
		// passed into the region.
		MmUnlockVadList(VadList);
		return STATUS_VA_NOT_AT_BASE;
	}
	
	ASSERT(!Vad->Flags.Committed);
	
	// It does! This means that the region can be dereserved with
	// this function.  Note that it does the job of unlocking the
	// VAD list.
	MiReleaseVad(Vad);
	return STATUS_SUCCESS;
}

PMMVAD MmLookUpVadByAddress(PMMVAD_LIST VadList, uintptr_t Address)
{
	// Look up the item in the VAD.
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&VadList->Tree, Address);
	
	if (!Entry)
	{
		// No entry found.
		return NULL;
	}
	
	PMMVAD Vad = (PMMVAD) Entry;
	
	// If the address is outside of the range of this VAD, then it doesn't
	// correspond to it. (In that case, really, LookUpItemApproximateRbTree
	// failed, since it's supposed to return items whose key is the greatest,
	// yet <= the starting address).
	//
	// Also check if the address is on the other side of the range.
	if (Address < Vad->Node.StartVa ||
		Address >= Vad->Node.StartVa + Vad->Node.Size * PAGE_SIZE)
	{
		// Nope, this VAD doesn't correspond to this region.
		return NULL;
	}
	
	return Vad;
}

#ifdef DEBUG
void MmDumpVadList(PMMVAD_LIST VadList)
{
	if (!VadList)
		VadList = &PsGetAttachedProcess()->VadList;
	
	MiLockVadList(VadList);
	PRBTREE_ENTRY Entry = GetFirstEntryRbTree(&VadList->Tree);

	DbgPrint("*** dumping VadList %p", VadList);
	for (; Entry != NULL; Entry = GetNextEntryRbTree(Entry))
	{
		PMMVAD Vad = CONTAINING_RECORD(Entry, MMVAD, Node.Entry);
		DbgPrint("\tStart: %p\tEnd: %zu", (void*)Vad->Node.StartVa, Vad->Node.Size);
	}

	MmUnlockVadList(VadList);
}
#endif

// Decommits a range of virtual memory in system space.
void MiDecommitVadInSystemSpace(PMMVAD Vad)
{
	MiLockVadList(&MiSystemVadList);
	MiDecommitVad(&MiSystemVadList, Vad, Vad->Node.StartVa, Vad->Node.Size);
}
