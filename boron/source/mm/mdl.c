/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	mm/mdl.c
	
Abstract:
	This module implements the memory descriptor list.
	
Author:
	iProgramInCpp - 21 January 2024
***/
#include <mm.h>
#include <ex.h>
#include <ps.h>

void MmUnmapPinnedPagesMdl(PMDL Mdl)
{
	if (!Mdl->MappedStartVA)
		return;
	
	MmUnmapPages(Mdl->Process->Pcb.PageMap, Mdl->MappedStartVA, Mdl->NumberPages);
	Mdl->Flags &= ~MDL_FLAG_MAPPED;
}

void MmUnpinPagesMdl(PMDL Mdl)
{
	MmUnmapPinnedPagesMdl(Mdl);
	
	if (~Mdl->Flags & MDL_FLAG_CAPTURED)
		return;
	
	for (size_t i = 0; i < Mdl->NumberPages; i++)
		MmFreePhysicalPage(Mdl->Pages[i]);
	
	Mdl->Flags &= ~MDL_FLAG_CAPTURED;
}

void MmFreeMdl(PMDL Mdl)
{
	MmUnmapPinnedPagesMdl(Mdl);
	MmUnpinPagesMdl(Mdl);
	MmFreePool(Mdl);
}

BSTATUS MmMapPinnedPagesMdl(PMDL Mdl, uintptr_t* OutAddress, uintptr_t Permissions)
{
	if (Mdl->MappedStartVA || Mdl->MapHandle)
		return STATUS_NO_REMAP;
	
	void* AddressV = NULL;
	uintptr_t MapAddress = 0;
	
	BIG_MEMORY_HANDLE Handle = MmAllocatePoolBig(
		POOL_FLAG_CALLER_CONTROLLED,
		Mdl->NumberPages,
		&AddressV,
		POOL_TAG("MdlM")
	);
	
	MapAddress = (uintptr_t) AddressV;
	
	// If the handle couldn't be obtained, we don't have enough
	// pool memory space, because the request was for a caller
	// controlled region.
	if (!Handle)
		return STATUS_INSUFFICIENT_MEMORY;
	
	uintptr_t Address = MapAddress;
	size_t Index = 0;
	
	HPAGEMAP PageMap = Mdl->Process->Pcb.PageMap;
	
	for (; Index < Mdl->NumberPages; Address += PAGE_SIZE, Index++)
	{
		// Add a reference to the page.
		MmPageAddReference(Mdl->Pages[Index]);
		
		if (!MmMapPhysicalPage(PageMap, Mdl->Pages[Index] * PAGE_SIZE, Address, Permissions | MM_PTE_ISFROMPMM))
		{
			// Unmap everything mapped so far.
			MmUnmapPages(PageMap, MapAddress, Index);
			
			// Unreference that page.
			MmFreePhysicalPage(Mdl->Pages[Index]);
			
			// Free the handle.
			MmFreePoolBig(Handle);
			
			return STATUS_INSUFFICIENT_MEMORY;
		}
	}
	
	Mdl->MappedStartVA = MapAddress;
	Mdl->MapHandle     = Handle;
	Mdl->Flags        |= MDL_FLAG_MAPPED;
	
	*OutAddress = MapAddress;
	
	return STATUS_SUCCESS;
}

PMDL MmAllocateMdl(uintptr_t VirtualAddress, size_t Length)
{
	// Figure out the number of pages to reserve the MDL for:
	uintptr_t StartPage = VirtualAddress & ~0xFFF;
	uintptr_t EndPage   = (VirtualAddress + Length + 0xFFF) & ~0xFFF;
	size_t NumPages = (EndPage - StartPage) / PAGE_SIZE;
	
	if (Length >= MDL_MAX_SIZE)
		return NULL;
	
	if (!MmIsAddressRangeValid(VirtualAddress, Length))
		return NULL;
	
	// Allocate enough space in pool memory.
	PMDL Mdl = MmAllocatePool(POOL_NONPAGED, sizeof(MDL) + NumPages * sizeof(int));
	if (!Mdl)
		return NULL;
	
	// Initialize the MDL
	Mdl->ByteOffset    = (short)(VirtualAddress & 0xFFF);
	Mdl->Flags         = 0;
	Mdl->Available     = 0; // pad
	Mdl->ByteCount     = Length;
	Mdl->SourceStartVA = VirtualAddress & ~0xFFF;
	Mdl->MappedStartVA = 0;
	Mdl->MapHandle     = POOL_NO_MEMORY_HANDLE;
	Mdl->Process       = PsGetCurrentProcess();
	Mdl->NumberPages   = NumPages;
	
	return Mdl;
}

BSTATUS MmProbeAndPinPagesMdl(PMDL Mdl)
{
	uintptr_t VirtualAddress = Mdl->SourceStartVA;
	size_t Size = Mdl->ByteCount;
	
	// Figure out the number of pages to reserve the MDL for:
	uintptr_t StartPage = VirtualAddress & ~0xFFF;
	uintptr_t EndPage   = (VirtualAddress + Size + 0xFFF) & ~0xFFF;
	BSTATUS FailureReason = STATUS_SUCCESS;
	
	if (Size >= MDL_MAX_SIZE)
		return STATUS_INVALID_PARAMETER;
	
	if (!MmIsAddressRangeValid(VirtualAddress, Size))
		return STATUS_INVALID_PARAMETER;
	
	HPAGEMAP PageMap = Mdl->Process->Pcb.PageMap;
	
	// TODO: Ensure proper failure if the whole buffer doesn't fit in system memory!
	
	size_t Index = 0;
	for (uintptr_t Address = StartPage; Address < EndPage; Address += PAGE_SIZE)
	{
		PMMPTE PtePtr = MmGetPTEPointer(PageMap, Address, false);
		
		bool TryFault = false;
		if (!PtePtr)
		{
			TryFault = true;
		}
		else
		{
			MMPTE Pte = *PtePtr;
			
			if (~Pte & MM_PTE_PRESENT)
			{
				TryFault = true;
			}
			else if (~Pte & MM_PTE_ISFROMPMM)
			{
				// Page isn't part of PMM, so fail
				FailureReason = STATUS_INVALID_PARAMETER;
				break;
			}
		}
		
		if (TryFault)
		{
			BSTATUS ProbeStatus = MmProbeAddress((void*) Address, sizeof(uintptr_t), true);
			
			if (ProbeStatus != STATUS_SUCCESS)
			{
				FailureReason = ProbeStatus;
				break;
			}
			
			// Success, so re-fetch the PTE:
			PtePtr = MmGetPTEPointer(PageMap, Address, false);
		}
		
		MMPTE Ptr = *PtePtr;
		
		// Fetch the page frame number.
		int Pfn = (Ptr & MM_PTE_ADDRESSMASK) / PAGE_SIZE;
		
		// Let the PF database know that the page was pinned.
		//
		// This should be safe to do as is, because the MDL process takes place while the
		// lock relevant to the part of the address space that the page belongs to is locked.
		MmPageAddReference(Pfn);
		
		// Register it in the MDL.
		ASSERT(Index < Mdl->NumberPages);
		Mdl->Pages[Index] = Pfn;
		Index++;
	}
	
	Mdl->Flags |= MDL_FLAG_CAPTURED;
	
	if (FailureReason)
	{
		Mdl->NumberPages = Index;
		MmUnpinPagesMdl(Mdl);
		return FailureReason;
	}
	
	return STATUS_SUCCESS;
}
