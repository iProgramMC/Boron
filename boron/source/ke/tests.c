/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	ke/tests.c
	
Abstract:
	This module implements some test routines for Boron.
	
Author:
	iProgramInCpp - 26 September 2023
***/

#include "../mm/mi.h" // horrible for now
#include <ex.h>

static void KepTestAllMemory(void* Memory, size_t Size)
{
	uint8_t* MemBytes = Memory;
	
	for (size_t i = 0; i < Size; i++)
	{
		MemBytes[i] = i ^ (i << 9) ^ (i << 42) ^ (i << 25);
	}
}

void KiPerformSlabAllocTest()
{
	//DbgPrint("Allocating a single uint32_t");
	uint32_t* Memory = MiSlabAllocate(true, sizeof(uint32_t));
	*Memory = 5;
	LogMsg("Allocated pointer %p of size %d, reading back %u", Memory, sizeof(uint32_t), *Memory);
	
	//DbgPrint("Freeing that uint32_t");
	
	MiSlabFree(Memory);
	
	//DbgPrint("Freed that uint32_t");
	
	//DbgPrint("Allocating a bunch of things");
	
	void* Mem16 = MiSlabAllocate(true, 16);
	void* Mem32 = MiSlabAllocate(true, 32);
	void* Mem64 = MiSlabAllocate(true, 64);
	void* Mem128 = MiSlabAllocate(true, 128);
	void* Mem256 = MiSlabAllocate(true, 256);
	void* Mem512 = MiSlabAllocate(true, 512);
	void* Mem1024_1 = MiSlabAllocate(true, 1024);
	void* Mem1024_2 = MiSlabAllocate(true, 1024);
	void* Mem1024_3 = MiSlabAllocate(true, 1024);
	void* Mem1024_4 = MiSlabAllocate(true, 1024);
	void* Mem1024_5 = MiSlabAllocate(true, 1024);
	void* Mem1024_6 = MiSlabAllocate(true, 1024);
	
	//DbgPrint("Testing the several allocations");
	
	KepTestAllMemory(Mem16, 16);
	KepTestAllMemory(Mem32, 32);
	KepTestAllMemory(Mem64, 64);
	KepTestAllMemory(Mem128, 128);
	KepTestAllMemory(Mem256, 256);
	KepTestAllMemory(Mem512, 512);
	KepTestAllMemory(Mem1024_1, 1024);
	KepTestAllMemory(Mem1024_2, 1024);
	KepTestAllMemory(Mem1024_3, 1024);
	KepTestAllMemory(Mem1024_4, 1024);
	KepTestAllMemory(Mem1024_5, 1024);
	KepTestAllMemory(Mem1024_6, 1024);
	
	//DbgPrint("Freeing everything");
	
	MiSlabFree(Mem16);
	MiSlabFree(Mem32);
	MiSlabFree(Mem64);
	MiSlabFree(Mem128);
	MiSlabFree(Mem256);
	MiSlabFree(Mem512);
	MiSlabFree(Mem1024_1);
	MiSlabFree(Mem1024_2);
	MiSlabFree(Mem1024_3);
	MiSlabFree(Mem1024_4);
	MiSlabFree(Mem1024_5);
	MiSlabFree(Mem1024_6);
	
	//DbgPrint("KiPerformSlabAllocTest Done");
}

void KiPerformPageMapTest()
{
	// Uncomment if you want it to fail, because page faults can't be taken at IPL_DPC.
	//KIPL OldIpl = KeRaiseIPL(IPL_DPC);
	
	HPAGEMAP pm = KeGetCurrentPageTable();
	
	if (MmMapAnonPage(pm, 0x5ADFDEADB000, MM_PTE_READWRITE | MM_PTE_SUPERVISOR, true))
	{
		*((uintptr_t*)0x5adfdeadbeef) = 420;
		LogMsg("[CPU %u] Read back from there: %p", KeGetCurrentPRCB()->LapicId, *((uintptr_t*)0x5adfdeadbeef));
		
		// Get rid of that shiza
		MmUnmapPages(pm, 0x5ADFDEADB000, 1);
		
		MmMapAnonPage(pm, 0x5ADFDEADD000, MM_PTE_READWRITE | MM_PTE_SUPERVISOR, true);
		MmUnmapPages (pm, 0x5ADFDEADD000, 1);
		
		// Try again!  We should get a page fault if we did everything correctly.
		//*((uintptr_t*)0x5adfdeadbeef) = 420;
		//LogMsg("[CPU %u] Read back from there: %p", KeGetCurrentPRCB()->LapicId, *((uintptr_t*)0x5adfdeadbeef));
	}
	else
	{
		LogMsg("Error, failed to map to %p", 0x5ADFDEADB000);
	}
	
	//KeLowerIPL(OldIpl);
}

void KiPerformPoolAllocTest()
{
	//uintptr_t Id = (uintptr_t) KeGetCurrentPRCB()->LapicId << 32;
	//MiDumpPoolInfo(Id);
	
	MIPOOL_SPACE_HANDLE Handle;
	void* Address = NULL;
	
	// Reserve a ton of pages. While we may not have 4 gigs of RAM, our
	// pool space is in the hundreds of gigabytes, so it's OK.
	// The memory pool only handles dishing out address _ranges_, not actual
	// usable memory.
	Handle = MiReservePoolSpaceTagged(1048576, &Address, MI_TAG("TEST"), 0);
	
	//MiDumpPoolInfo(Id);
	LogMsg("[CPU %d] MiReservePoolSpaceTagged returned: %p", KeGetCurrentPRCB()->LapicId, Handle);
	
	MiFreePoolSpace(Handle);
	
	//MiDumpPoolInfo(Id);
}

void KiPerformExPoolTest()
{
	void* Address = NULL;
	BIG_MEMORY_HANDLE Handle = MmAllocatePoolBig(0, 42, &Address, MI_TAG("Tst!"));
	
	if (!Handle)
	{
		LogMsg("Error, MmAllocatePoolBig didn't return a handle");
		return;
	}
	
	KepTestAllMemory(Address, 42 * PAGE_SIZE);
	
	LogMsg("[CPU %d] ExPoolTest SUCCESS", KeGetCurrentPRCB()->LapicId);
	
	MmFreePoolBig(Handle);
}

static void KepDpcTest(UNUSED PKDPC Dpc, void* Context, void* SystemArgument1, void* SystemArgument2)
{
	LogMsg("KepDpcTest!  Context = %p, SysArg1 = %p, SysArg2 = %p", Context, SystemArgument1, SystemArgument2);
}

void KiPerformDpcTest()
{
	static KDPC Dpc;
	
	// Test will only run on the bootstrap processor.
	if (!KeGetCurrentPRCB()->IsBootstrap)
		return;
	
	// Initialize the DPC with some testing values.
	KeInitializeDpc(&Dpc, KepDpcTest, (void*) 0x0123456789ABCDEF);
	
	// Set it as important because we don't have automatic dispatch yet.
	KeSetImportantDpc(&Dpc, true);
	
	// Enqueue!
	KeEnqueueDpc(&Dpc, (void*) 0xAEAEAEAEEAEAEAEA, (void*) 0x9879876546543210);
}

void KiPerformDelayedIntTest()
{
	LogMsg("KiPerformDelayedIntTest:  Sending off an interrupt in one second.  The printed times are for the TSC and HPET respectively.");
	
	// Delay it by one second.
	uint64_t Ticks = 1 * HalGetIntTimerFrequency();
	
	if (!HalUseOneShotIntTimer())
	{
		LogMsg("Sorry, one shot timer isn't actually supported");
		return;
	}
	
	HalRequestInterruptInTicks(Ticks);
}

void KiPerformTests()
{
	KiPerformSlabAllocTest();
	KiPerformPoolAllocTest();
	KiPerformExPoolTest();
	KiPerformDpcTest();
	KiPerformDelayedIntTest();
	KiPerformPageMapTest();
	LogMsg("CPU %d finished all tests", KeGetCurrentPRCB()->LapicId);
}
