/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mm/pmm.c
	
Abstract:
	This module contains the implementation for the virtual
	memory manager in Boron. This currently includes the
	TLB shootdown request thunk and the page fault "handler".
	
Author:
	iProgramInCpp - 28 August 2023
***/

#include "mi.h"

static KSPIN_LOCK MmpKernelSpaceLock;

// forces all cores to issue a TLB shootdown (invalidate the address from the
// TLB - with invlpg on amd64 for instance)
void MmIssueTLBShootDown(uintptr_t Address, size_t Length)
{
	KeIssueTLBShootDown(Address, Length);
}

HPAGEMAP MmGetCurrentPageMap()
{
	return (HPAGEMAP) KeGetCurrentPageTable();
}

void MmInitAllocators()
{
	MiInitSlabs();
	MiInitPool();
}

KIPL MmLockKernelSpace()
{
	KIPL ReturnIpl;
	KeAcquireSpinLock(&MmpKernelSpaceLock, &ReturnIpl);
	return ReturnIpl;
}

void MmUnlockKernelSpace(KIPL OldIpl)
{
	KeReleaseSpinLock(&MmpKernelSpaceLock, OldIpl);
}
