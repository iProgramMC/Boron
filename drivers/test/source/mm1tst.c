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

uint8_t PortReadByte(uint16_t portNo)
{
    uint8_t rv;
    ASM("inb %1, %0" : "=a" (rv) : "dN" (portNo));
    return rv;
}

void PortWriteByte(uint16_t portNo, uint8_t data)
{
	ASM("outb %0, %1"::"a"((uint8_t)data),"Nd"((uint16_t)portNo));
}

void Reboot() {
	
    uint8_t good = 0x02;
    while (good & 0x02)
        good = PortReadByte(0x64);
    PortWriteByte(0x64, 0xFE);
	
	(void) KeDisableInterrupts();
	while (true) KeWaitForNextInterrupt();
}

void PerformDemandPageTest()
{
	LogMsg(">> Demand page test");
	HPAGEMAP Map = MiGetCurrentPageMap();
	
	void* PoolAddr = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, 1, POOL_TAG("Mts1"));
	ASSERT(PoolAddr);
	
	uintptr_t Va = (uintptr_t) PoolAddr;
	
	// Make the PTE a demand page PTE.
	MmLockKernelSpaceExclusive();
	
	PMMPTE Pte = MiGetPTEPointer(Map, Va, true);
	ASSERT(Pte);
	*Pte = MM_DPTE_DEMANDPAGED | MM_PTE_READWRITE;
	
	MmUnlockKernelSpace();
	
	// This should work.
	*((volatile uint32_t*)Va) = 0x87654321;
	
	LogMsg("Va read: %08x", *((uint32_t*)Va));
	
	MmLockKernelSpaceExclusive();
	MiUnmapPages(Map, Va, 1);
	MmUnlockKernelSpace();
	
	MmFreePoolBig(PoolAddr);
}

void PerformCopyOnWriteTest()
{
	LogMsg(">> Copy on write test");
	HPAGEMAP Map = MiGetCurrentPageMap();
	
	void* PoolAddr = MmAllocatePoolBig(POOL_FLAG_CALLER_CONTROLLED, 2, POOL_TAG("Mts2"));
	ASSERT(PoolAddr);
	
	uintptr_t Va1 = (uintptr_t) PoolAddr;
	uintptr_t Va2 = Va1 + PAGE_SIZE;
	
	// Allocate 1 singular physical page, and add an extra reference to it.
	int Pfn = MmAllocatePhysicalPage();
	ASSERT(Pfn != PFN_INVALID);
	MmPageAddReference(Pfn);
	
	// Place something in that page
	*((volatile uint32_t*)MmGetHHDMOffsetAddr(MmPFNToPhysPage(Pfn))) = 0x12345678;
	
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
	*((volatile uint32_t*)Va2) = 0x87654321;
	
	// Read from them again.
	LogMsg("Va1 read 2: %08x", *((uint32_t*)Va1));
	LogMsg("Va2 read 2: %08x", *((uint32_t*)Va2));
	
	// Unmap everything.
	MmLockKernelSpaceExclusive();
	MiUnmapPages(Map, Va1, 2);
	MmUnlockKernelSpace();
	
	// Free the pool space.
	MmFreePoolBig(PoolAddr);
}

void PerformMm1Test()
{
	PerformDemandPageTest();
	PerformCopyOnWriteTest();
	
	// I needed this code to reboot the VM when I was finding some race conditions
	//DbgPrint("REBOOT!");
	//Reboot();
}
