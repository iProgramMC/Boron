/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm/commit.c
	
Abstract:
	This module defines functions related to virtual memory
	commit/decommit.
	
Author:
	iProgramInCpp - 9 March 2025
***/
#include <mm.h>
#include <ex.h>

//
// Commits a range of virtual memory, with anonymous pages.
//
// The entire memory region must be uncommitted.
//
BSTATUS MmCommitVirtualMemory(uintptr_t StartVa, size_t StartPages)
{
	return STATUS_UNIMPLEMENTED;
}
