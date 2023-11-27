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


void* ExAllocateSmall(size_t Size)
{
	return MiSlabAllocate(true, Size);
}

void ExFreeSmall(void* Pointer, UNUSED size_t Size)
{
	return MiSlabFree(Pointer);
}
