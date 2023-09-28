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
	//SLogMsg("Allocating a single uint32_t");
	uint32_t* Memory = MiSlabAllocate(sizeof(uint32_t));
	*Memory = 5;
	LogMsg("Allocated pointer %p of size %d, reading back %u", Memory, sizeof(uint32_t), *Memory);
	
	//SLogMsg("Freeing that uint32_t");
	
	MiSlabFree(Memory, sizeof(uint32_t));
	
	//SLogMsg("Freed that uint32_t");
	
	//SLogMsg("Allocating a bunch of things");
	
	void* Mem16 = MiSlabAllocate(16);
	void* Mem32 = MiSlabAllocate(32);
	void* Mem64 = MiSlabAllocate(64);
	void* Mem128 = MiSlabAllocate(128);
	void* Mem256 = MiSlabAllocate(256);
	void* Mem512 = MiSlabAllocate(512);
	void* Mem1024_1 = MiSlabAllocate(1024);
	void* Mem1024_2 = MiSlabAllocate(1024);
	void* Mem1024_3 = MiSlabAllocate(1024);
	void* Mem1024_4 = MiSlabAllocate(1024);
	void* Mem1024_5 = MiSlabAllocate(1024);
	void* Mem1024_6 = MiSlabAllocate(1024);
	
	//SLogMsg("Testing the several allocations");
	
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
	
	//SLogMsg("Freeing everything");
	
	MiSlabFree(Mem16, 16);
	MiSlabFree(Mem32, 32);
	MiSlabFree(Mem64, 64);
	MiSlabFree(Mem128, 128);
	MiSlabFree(Mem256, 256);
	MiSlabFree(Mem512, 512);
	MiSlabFree(Mem1024_1, 1024);
	MiSlabFree(Mem1024_2, 1024);
	MiSlabFree(Mem1024_3, 1024);
	MiSlabFree(Mem1024_4, 1024);
	MiSlabFree(Mem1024_5, 1024);
	MiSlabFree(Mem1024_6, 1024);
	
	//SLogMsg("KiPerformSlabAllocTest Done");
}

void KiPerformPageMapTest()
{
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
		*((uintptr_t*)0x5adfdeadbeef) = 420;
		
		LogMsg("[CPU %u] Read back from there: %p", KeGetCurrentPRCB()->LapicId, *((uintptr_t*)0x5adfdeadbeef));
	}
	else
	{
		LogMsg("Error, failed to map to %p", 0x5ADFDEADB000);
	}
}

void KiPerformPoolAllocTest()
{
	uintptr_t Id = (uintptr_t) KeGetCurrentPRCB()->LapicId << 32;
	MiDumpPoolInfo(Id);
	
	MIPOOL_SPACE_HANDLE Handle;
	void* Address = NULL;
	
	// Reserve a fuck ton of pages
	Handle = MiReservePoolSpaceTagged(1048576, &Address, MI_TAG("TEST"));
	
	MiDumpPoolInfo(Id);
	
	MiFreePoolSpace(Handle);
	
	MiDumpPoolInfo(Id);
}

void KiPerformTests()
{
	KiPerformSlabAllocTest();
	KiPerformPoolAllocTest();
	//KiPerformPageMapTest();
	LogMsg("CPU %d finished all tests", KeGetCurrentPRCB()->LapicId);
}
