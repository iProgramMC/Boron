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

BSTATUS MmCaptureMdl(PMDL Mdl, uintptr_t VirtualAddress, size_t Size)
{
	// Initialize the MDL
	Mdl->ByteOffset    = (short)(VirtualAddress & 0xFFF);
	Mdl->Flags         = 0;
	Mdl->Available     = 0; // pad
	Mdl->ByteCount     = Size;
	Mdl->MappedStartVA = VirtualAddress & ~0xFFF;
	Mdl->Pages         = NULL;
	Mdl->NumberPages   = 0;
	Mdl->Process       = PsGetCurrentProcess();
	
	uintptr_t StartPage = VirtualAddress & ~0xFFF;
	uintptr_t EndPage   = (VirtualAddress + Size + 0xFFF) & ~0xFFF;
	
	
	return STATUS_UNIMPLEMENTED;
}
