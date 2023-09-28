/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ex/pool.c
	
Abstract:
	This module contains the implementation of the
	executive's pool allocator.
	
Author:
	iProgramInCpp - 27 September 2023
***/
#include <ex.h>
#include "../mm/mi.h"

void* ExGetAddressFromHandle(EXMEMORY_HANDLE Handle)
{
	return MiGetAddressFromPoolSpaceHandle((MIPOOL_SPACE_HANDLE)Handle);
}

EXMEMORY_HANDLE ExAllocatePool(int PoolFlags, size_t SizeInPages, void** OutputAddress, int Tag)
{
	EXMEMORY_HANDLE OutHandle = (EXMEMORY_HANDLE) MiReservePoolSpaceTagged (SizeInPages, OutputAddress, Tag);
	
	if (!OutHandle)
		return OutHandle;
	
	// TODO
	
	return OutHandle;
}

void ExFreePool(EXMEMORY_HANDLE Handle)
{
	// TODO
	
	MiFreePoolSpace((MIPOOL_SPACE_HANDLE) Handle);
}