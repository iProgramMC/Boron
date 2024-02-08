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

BSTATUS MmCaptureMdl(PMDL* MdlOut, uintptr_t VirtualAddress, size_t Size)
{	
	// Figure out the number of pages to reserve the MDL for:
	uintptr_t StartPage = VirtualAddress & ~0xFFF;
	uintptr_t EndPage   = (VirtualAddress + Size + 0xFFF) & ~0xFFF;
	size_t NumPages = (EndPage - StartPage) / PAGE_SIZE;
	BSTATUS FailureReason = STATUS_SUCCESS;
	uintptr_t LastSuccessAddress = 0;
	
	if (Size >= MDL_MAX_SIZE)
		return STATUS_INVALID_PARAMETER;
	
	if (VirtualAddress > MM_USER_SPACE_END || VirtualAddress + Size > MM_USER_SPACE_END)
		return STATUS_INVALID_PARAMETER;
	
	if (!MmIsAddressRangeValid(VirtualAddress, Size))
		return STATUS_INVALID_PARAMETER;
	
	// Allocate enough space in pool memory.
	PMDL Mdl = MmAllocatePool(POOL_NONPAGED, sizeof(MDL) + NumPages * sizeof(int));
	if (!Mdl)
		return STATUS_INSUFFICIENT_MEMORY;
	
	// Initialize the MDL
	Mdl->ByteOffset    = (short)(VirtualAddress & 0xFFF);
	Mdl->Flags         = 0;
	Mdl->Available     = 0; // pad
	Mdl->ByteCount     = Size;
	Mdl->MappedStartVA = VirtualAddress & ~0xFFF;
	Mdl->Process       = PsGetCurrentProcess();
	Mdl->NumberPages   = NumPages;
	
	HPAGEMAP PageMap = MmGetCurrentPageMap();
	
	int Index = 0;
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
		Mdl->Pages[Index] = Pfn;
		Index++;
		
		LastSuccessAddress = Address;
	}
	
	if (FailureReason)
	{
		// Roll back any performed pinning:
		int Index = 0;
		for (uintptr_t Address = StartPage; Address < LastSuccessAddress; Address += PAGE_SIZE)
		{
			int Pfn = Mdl->Pages[Index];
			Index++;
			
			// Free the reference to the physical page.  This will not expunge
			// the PFN from physical memory, because we added a reference to it
			// above.
			MmFreePhysicalPage(Pfn);
		}
		
		MmFreePool(Mdl);
		return FailureReason;
	}
	
	*MdlOut = Mdl;
	
	return STATUS_SUCCESS;
}
