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
#include <mm.h>
#include <ex.h>

static inline void MiLockVadList(PMMVAD_LIST VadList)
{
	UNUSED BSTATUS Status = KeWaitForSingleObject(&VadList->Mutex, false, TIMEOUT_INFINITE, MODE_KERNEL);
	ASSERT(Status == STATUS_SUCCESS);
}

PMMVAD_LIST MmLockVadListProcess(PEPROCESS Process)
{
	MiLockVadList(&Process->VadList);
	return &Process->VadList;
}

void MmUnlockVadList(PMMVAD_LIST VadList)
{
	KeReleaseMutex(&VadList->Mutex);
}

void MmInitializeVadList(PMMVAD_LIST VadList)
{
	KeInitializeMutex(&VadList->Mutex, MM_VAD_MUTEX_LEVEL);
	InitializeRbTree(&VadList->Tree);
}

// Reserves a range of virtual memory.
BSTATUS MmReserveVirtualMemory(size_t SizePages, void** OutAddress, int AllocationType, int Protection)
{
	if (Protection & ~(PAGE_READ | PAGE_WRITE | PAGE_EXECUTE))
		return STATUS_INVALID_PARAMETER;
	
	if (AllocationType & ~(MEM_RESERVE | MEM_COMMIT | MEM_SHARED | MEM_TOP_DOWN))
		return STATUS_INVALID_PARAMETER;
	
	PEPROCESS Process = PsGetCurrentProcess();
	PMMADDRESS_NODE AddrNode;
	
	BSTATUS Status = MmAllocateAddressSpace(&Process->Heap, SizePages, AllocationType & MEM_TOP_DOWN, &AddrNode);
	if (FAILED(Status))
		return Status;
	
	PMMVAD Vad = (PMMVAD) AddrNode;
	MiLockVadList(&Process->VadList);
	
	if (!InsertItemRbTree(&Process->VadList.Tree, &Vad->Node.Entry))
	{
		// Which status should we return here?
	#ifdef DEBUG
		KeCrash("MmReserveVirtualMemory: Failed to insert VAD into VAD list (address %p)", Vad->Node.StartVa);
	#endif
		MmUnlockVadList(&Process->VadList);
		return STATUS_INSUFFICIENT_VA_SPACE;
	}
	
	// Clear all the fields in the VAD.
	Vad->Flags.LongFlags  = 0;
	Vad->Mapped.Object    = NULL;
	Vad->OffsetInFile     = 0;
	Vad->Flags.Committed  =  (AllocationType & MEM_COMMIT) != 0;
	Vad->Flags.Private    = (~AllocationType & MEM_SHARED) != 0;
	Vad->Flags.Protection = Protection;
	
	*OutAddress = (void*) Vad->Node.StartVa;
	
	MmUnlockVadList(&Process->VadList);
	return STATUS_SUCCESS;
}

// Releases a range of virtual memory represented by a VAD.
// The range must be entirely decommitted before it is de-reserved.
// The VAD list lock must be held before calling the function.
BSTATUS MiReleaseVad(PMMVAD Vad)
{
	PEPROCESS Process = PsGetCurrentProcess();

	// Step 1.  Remove the VAD from the VAD list tree.
	RemoveItemRbTree(&Process->VadList.Tree, &Vad->Node.Entry);
	MmUnlockVadList(&Process->VadList);
	
	// Step 2.  Zero out all of the PTEs.
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
		
		// Assume that the page is present.
		ASSERT(~*Pte & MM_PTE_PRESENT);
		
		*Pte = 0;
		Pte++;
		CurrentVa += PAGE_SIZE;
	}
	
	// Step 3.
	MiFreeUnusedMappingLevelsInCurrentMap(Vad->Node.StartVa, Vad->Node.Size);
	
	MmUnlockSpace(Ipl, Vad->Node.StartVa);
	
	// Step 4. Finally, add the range into the heap/free list.
	BSTATUS Status = MmFreeAddressSpace(&Process->Heap, &Vad->Node);
	if (FAILED(Status))
	{
		// NOTE: This should never happen.  But perhaps it's a good time
		// to consider that we should probably own both the VAD list mutex
		// and the heap mutex at the same time, with a single
		// KeWaitForMultipleObjects call.
		//
		// An argument against it is that there's no way for this region
		// to be used if it's not part of either the heap or the VAD list.
		
	#ifdef DEBUG
		KeCrash("MmDereserveVad: Failed to insert VAD into free-heap (address %p)", Vad->Node.StartVa);
	#endif
		
		// Re-insert it?
		MiLockVadList(&Process->VadList);
		bool Inserted = InsertItemRbTree(&Process->VadList.Tree, &Vad->Node.Entry);
		MmUnlockVadList(&Process->VadList);
		
		if (!Inserted)
		{
			// Ohhh.... crap
		#ifdef DEBUG
			KeCrash("MmDereserveVad: Unreachable");
		#endif
			Status = STATUS_UNIMPLEMENTED;
		}
	}
	
	return Status;
}

// This releases a range of virtual memory allocated using MmReserveVirtualMemoryVad.
// The address must point to the base of the region.
// Note that the entire range must have been decommitted first.
BSTATUS MmReleaseVirtualMemory(void* Address)
{
	uintptr_t AddressI = (uintptr_t) Address;
	
	PMMVAD_LIST VadList = MmLockVadListProcess(PsGetCurrentProcess());
	
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
	return MiReleaseVad(Vad);
}

PMMVAD MmLookUpVadByAddress(PMMVAD_LIST VadList, uintptr_t Address)
{
	// Look up the item in the VAD.
	PRBTREE_ENTRY Entry = LookUpItemApproximateRbTree(&VadList->Tree, Address);
	
	if (!Entry)
	{
		DbgPrint("No entry for address %p", Address);
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
