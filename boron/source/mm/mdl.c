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
	return STATUS_UNIMPLEMENTED;
	
	// Figure out the number of pages to reserve the MDL for:
	uintptr_t StartPage = VirtualAddress & ~0xFFF;
	uintptr_t EndPage   = (VirtualAddress + Size + 0xFFF) & ~0xFFF;
	size_t NumPages = (EndPage - StartPage) / PAGE_SIZE;
	
	// Allocate enough space in pool memory.
	PMDL Mdl = MmAllocatePool(POOL_NONPAGED, sizeof(MDL) + NumPages * sizeof(Mdl->Pages[0]));
	if (!Mdl)
		return STATUS_INSUFFICIENT_MEMORY;
	
	*MdlOut = Mdl;
	
	// Initialize the MDL
	Mdl->ByteOffset    = (short)(VirtualAddress & 0xFFF);
	Mdl->Flags         = 0;
	Mdl->Available     = 0; // pad
	Mdl->ByteCount     = Size;
	Mdl->MappedStartVA = VirtualAddress & ~0xFFF;
	Mdl->NumberPages   = 0;
	Mdl->Process       = PsGetCurrentProcess();
	Mdl->NumberPages   = NumPages;
	
	
	
	return STATUS_UNIMPLEMENTED;
}
