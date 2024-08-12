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
	
	MiUnmapPages(Mdl->Process->Pcb.PageMap, Mdl->MappedStartVA, Mdl->NumberPages);
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

BSTATUS MmMapPinnedPagesMdl(PMDL Mdl, void** OutAddress, uintptr_t Permissions)
{
	if (Mdl->MappedStartVA)
		return STATUS_NO_REMAP;
	
	void* AddressV = MmAllocatePoolBig(
		POOL_FLAG_CALLER_CONTROLLED,
		Mdl->NumberPages,
		POOL_TAG("MdlM")
	);
	
	uintptr_t MapAddress = (uintptr_t) AddressV;
	
	// If the memory couldn't be allocated, we don't have enough
	// pool memory space, because the request was for a caller
	// controlled region.
	if (!AddressV)
		return STATUS_INSUFFICIENT_MEMORY;
	
	uintptr_t Address = MapAddress;
	size_t Index = 0;
	
	HPAGEMAP PageMap = Mdl->Process->Pcb.PageMap;
	
	for (; Index < Mdl->NumberPages; Address += PAGE_SIZE, Index++)
	{
		// Add a reference to the page.
		MmPageAddReference(Mdl->Pages[Index]);
		
		if (!MiMapPhysicalPage(PageMap, Mdl->Pages[Index] * PAGE_SIZE, Address, Permissions | MM_PTE_ISFROMPMM))
		{
			// Unmap everything mapped so far.
			MiUnmapPages(PageMap, MapAddress, Index);
			
			// Unreference that page.
			MmFreePhysicalPage(Mdl->Pages[Index]);
			
			// Free the handle.
			MmFreePoolBig(AddressV);
			
			return STATUS_INSUFFICIENT_MEMORY;
		}
	}
	
	Mdl->MappedStartVA = MapAddress;
	Mdl->Flags        |= MDL_FLAG_MAPPED;
	
	*OutAddress = AddressV;
	
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
	
	// TODO: maybe this is inadequate... though MmProbeAndPinPagesMdl does do proper checks
	if (!MmIsAddressRangeValid(VirtualAddress, Length, MODE_KERNEL))
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
	Mdl->Process       = PsGetCurrentProcess();
	Mdl->NumberPages   = NumPages;
	
	return Mdl;
}

BSTATUS MmProbeAndPinPagesMdl(PMDL Mdl, KPROCESSOR_MODE AccessMode, bool IsWrite)
{
	uintptr_t VirtualAddress = Mdl->SourceStartVA;
	size_t Size = Mdl->ByteCount;
	
	// Figure out the number of pages to reserve the MDL for:
	uintptr_t StartPage = VirtualAddress & ~0xFFF;
	uintptr_t EndPage   = (VirtualAddress + Size + 0xFFF) & ~0xFFF;
	BSTATUS FailureReason = STATUS_SUCCESS;
	
	if (Size >= MDL_MAX_SIZE)
		return STATUS_INVALID_PARAMETER;
	
	if (!MmIsAddressRangeValid(VirtualAddress, Size, AccessMode))
		return STATUS_INVALID_PARAMETER;
	
	if (AccessMode == MODE_USER && MM_USER_SPACE_END < EndPage)
		return STATUS_INVALID_PARAMETER;
	
	HPAGEMAP PageMap = Mdl->Process->Pcb.PageMap;
	
	// Fault all the pages in.
	for (uintptr_t Address = StartPage; Address < EndPage; Address += PAGE_SIZE)
	{
		BSTATUS ProbeStatus = MmProbeAddress((void*) Address, sizeof(uintptr_t), IsWrite, AccessMode);
		
		if (ProbeStatus != STATUS_SUCCESS)
		{
			FailureReason = ProbeStatus;
			break;
		}
	}
	
	if (FailureReason)
		return FailureReason;
	
	// TODO(WORKINGSET): Check if the working set (when we add it) can even fit all of these pages.
	// TODO(possibly related to above): Ensure proper failure if the whole buffer doesn't fit in system memory!
	
	size_t Index = 0;
	for (uintptr_t Address = StartPage; Address < EndPage; Address += PAGE_SIZE)
	{
		while (true)
		{
			MmLockSpaceShared(Address);
			PMMPTE PtePtr = MiGetPTEPointer(PageMap, Address, false);
			
			bool TryFault = false;
			if (!PtePtr)
			{
				// If there is no PTE, try faulting on that address.  But ideally, there should, as we
				// faulted everything in previously.
				TryFault = true;
			}
			else
			{
				MMPTE Pte = *PtePtr;
				
				if (~Pte & MM_PTE_PRESENT)
				{
					// Try to fault on this page.  We don't know what kind of non-present page this is.
					TryFault = true;
				}
				else if (~Pte & MM_PTE_ISFROMPMM)
				{
					// This is MMIO space or the HHDM.  Disallow its capture.
					FailureReason = STATUS_INVALID_PARAMETER;
					MmUnlockSpace(Address);
					break;
				}
				else if ((~Pte & MM_PTE_READWRITE) && IsWrite)
				{
					// This is a read-only page and we are trying to write to it.  
					TryFault = true;
				}
			}
			
			if (TryFault)
			{
				MmUnlockSpace(Address);
				
				BSTATUS ProbeStatus = MmProbeAddress((void*) Address, sizeof(uintptr_t), IsWrite, AccessMode);
				
				if (ProbeStatus != STATUS_SUCCESS)
				{
					FailureReason = ProbeStatus;
					break;
				}
				
				// Continue the loop.  This will re-lock the address space at the start of the next
				// iteration, re-fetch the PTE and try this whole ordeal again.
				continue;
			}
			
			ASSERT(PtePtr);
			
			MMPTE Pte = *PtePtr;
			
			// Probe was successful and this is a proper page (writable if IsWrite is true).
			ASSERT((Pte & MM_PTE_READWRITE) || !IsWrite);
			ASSERT(Pte & MM_PTE_ISFROMPMM);
			ASSERT(Pte & MM_PTE_PRESENT);
			
			// Fetch the page frame number.
			int Pfn = (Pte & MM_PTE_ADDRESSMASK) / PAGE_SIZE;
			
			// Let the PF database know that the page was pinned.
			//
			// This should be safe to do as is, because the MDL process takes place while the
			// lock relevant to the part of the address space that the page belongs to is locked.
			MmPageAddReference(Pfn);
			
			// Register it in the MDL.
			ASSERT(Index < Mdl->NumberPages);
			Mdl->Pages[Index] = Pfn;
			Index++;
			
			// Unlock the address space and break out of the probe loop.  This will continue the
			// capture process.
			MmUnlockSpace(Address);
			break;
		}
		
		if (FailureReason)
			break;
	}
	
	Mdl->Flags |= MDL_FLAG_CAPTURED;
	
	if (FailureReason)
	{
		// Unpin only up to the current index, the rest weren't filled in due to the failure.
		Mdl->NumberPages = Index;
		MmUnpinPagesMdl(Mdl);
		return FailureReason;
	}
	
	return STATUS_SUCCESS;
}
