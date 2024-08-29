/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ccbtst.c
	
Abstract:
	This module implements the MM1 test for the test driver.
	It tests the functionality of the page fault handler.
	
Author:
	iProgramInCpp - 29 August 2024
***/
#include <ke.h>
#include <mm.h>
#include "utils.h"

// NOTE: This is not ideal.  The test penetrates into internal functions.

void PerformCopyOnWriteTest()
{
	HPAGEMAP Map = MiGetCurrentPageMap();
	
	void* PoolAddr = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, 2, POOL_TAG("Mtst"));
	ASSERT(PoolAddr);
	
	uintptr_t Va1 = (uintptr_t) PoolAddr;
	uintptr_t Va2 = Va1 + PAGE_SIZE;
	
	// Allocate 1 singular physical page, and add an extra reference to it.
	int Pfn = MmAllocatePhysicalPage();
	ASSERT(Pfn != PFN_INVALID);
	MmPageAddReference(Pfn);
	
	// Place something in that page
	*((uint32_t*)MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn))) = 0x12345678;
	
	// Now map them in.
	MmLockKernelSpaceExclusive();
	
	bool Res1 = MiMapPhysicalPage(Map, MmPFNToPhysPage(Pfn), Va1, MM_PTE_ISFROMPMM | MM_PTE_COW);
	bool Res2 = MiMapPhysicalPage(Map, MmPFNToPhysPage(Pfn), Va2, MM_PTE_ISFROMPMM | MM_PTE_COW);
	ASSERT(Res1 && Res2);
	
	MmUnlockKernelSpace();
	
	// Read from them
	LogMsg("Va1 read 1: %08x", *((uint32_t*)Va1));
	LogMsg("Va2 read 1: %08x", *((uint32_t*)Va2));
	
	// Write a value to Va2.  It shouldn't reflect in Va1.
	*((uint32_t*)Va2) = 0x87654321;
	
	// Read from them again.
	LogMsg("Va1 read 2: %08x", *((uint32_t*)Va1));
	LogMsg("Va2 read 2: %08x", *((uint32_t*)Va2));
}

void PerformMm1Test()
{
	PerformCopyOnWriteTest();
}
