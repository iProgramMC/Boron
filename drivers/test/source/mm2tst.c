/***
	The Boron Operating System
	Copyright (C) 2025 iProgramInCpp

Module name:
	mm2tst.c
	
Abstract:
	This module implements the MM2 test for the test driver.
	It tests the functionality of MmReserveVirtualMemory and
	MmCommitVirtualMemory.
	
Author:
	iProgramInCpp - 9 March 2025
***/
#include <ke.h>
#include <mm.h>
#include <ex.h>
#include "utils.h"

#define CHECK_FAIL(desc) do { if (FAILED(Status)) KeCrash("MM2[%d]: Failed to %s with status %d", __LINE__, desc, Status); } while (0)

// Note : These aren't the actual APIs userspace sees.

void Mm2BasicReserveTest()
{
	// Test a basic case, where you just reserve one range of memory and then free it.
	void* Address = NULL;
	BSTATUS Status;
	
	Status = MmReserveVirtualMemory(400, &Address, MEM_RESERVE, 0);
	CHECK_FAIL("reserve virtual memory");
	
	// Let's reserve another!
	void* Address2 = NULL;
	Status = MmReserveVirtualMemory(40000, &Address2, MEM_RESERVE, 0);
	CHECK_FAIL("reserve virtual memory");
	
	LogMsg("Memory reserved at %p(#1) and %p(#2)", Address, Address2);
	
	// Free the first one and try to reserve a bigger one in its place.
	Status = MmReleaseVirtualMemory(Address);
	CHECK_FAIL("release virtual memory");
	
	Status = MmReserveVirtualMemory(400000, &Address, MEM_RESERVE, 0);
	CHECK_FAIL("reserve virtual memory");
	
	LogMsg("New #1 address: %p", Address);
	
	// Reallocate the second one with the same size.
	Status = MmReleaseVirtualMemory(Address2);
	CHECK_FAIL("release virtual memory");
	
	Status = MmReserveVirtualMemory(40000, &Address2, MEM_RESERVE, 0);
	CHECK_FAIL("reserve virtual memory");
	
	LogMsg("New #2 address: %p", Address2);
	
	MmDebugDumpVad();
	
	// The end
	Status = MmReleaseVirtualMemory(Address);
	CHECK_FAIL("release virtual memory");
	
	Status = MmReleaseVirtualMemory(Address2);
	CHECK_FAIL("release virtual memory");
}

void Mm2BasicCommitTest()
{
	const size_t PageCount = 400;
	
	// Reserve some space, then commit it, test out the new committed region, decommit and release.
	void* Address = NULL;
	BSTATUS Status;
	
	Status = MmReserveVirtualMemory(PageCount, &Address, MEM_RESERVE, PAGE_READ | PAGE_WRITE);
	CHECK_FAIL("reserve virtual memory");
	
	LogMsg("Reserved at %p", Address);
	
	Status = MmCommitVirtualMemory((uintptr_t) Address, PageCount, 0);
	CHECK_FAIL("commit virtual memory");
	
	// Dump the first U64
	LogMsg("First U64: %p", *((uintptr_t*)Address));
	
	uint8_t* AddressU8 = Address;
	for (size_t i = 0; i < PageCount * PAGE_SIZE; i++)
	{
		// Fill it with random data
		AddressU8[i] = i + i * i + i * i * i;
	}
	
	// Dump the first U64 again
	LogMsg("First U64: %p", *((uintptr_t*)Address));
	MmDebugDumpVad();
	
	Status = MmDecommitVirtualMemory((uintptr_t) Address, PageCount);
	CHECK_FAIL("decommit virtual memory");
	
	Status = MmReleaseVirtualMemory(Address);
	CHECK_FAIL("release virtual memory");
}

void Mm2TopDownMemoryTest()
{
	void *Address1, *Address2;
	BSTATUS Status;
	
	Status = MmReserveVirtualMemory(400, &Address1, MEM_RESERVE | MEM_TOP_DOWN, 0);
	CHECK_FAIL("reserve virtual memory");
	
	Status = MmReserveVirtualMemory(400, &Address2, MEM_RESERVE | MEM_TOP_DOWN, 0);
	CHECK_FAIL("reserve virtual memory");
	
	LogMsg("Address 1: %p, Address 2: %p", Address1, Address2);
	MmDebugDumpVad();
	MmDebugDumpHeap();
	
	Status = MmReleaseVirtualMemory(Address1);
	CHECK_FAIL("release virtual memory");
	
	Status = MmReleaseVirtualMemory(Address2);
	CHECK_FAIL("release virtual memory");
}

void Mm2ExposedApiTest()
{
	BSTATUS Status;
	void* Address = NULL;
	size_t RegionSize = 40000000;
	
	Status = OSAllocateVirtualMemory(CURRENT_PROCESS_HANDLE, &Address, &RegionSize, MEM_RESERVE | MEM_COMMIT, PAGE_READ | PAGE_WRITE);
	CHECK_FAIL("allocate virtual memory");
	
	LogMsg("OSAllocateVirtualMemory returned address %p and reg size %zu", Address, RegionSize);
	
	MmDebugDumpVad();
	
	uint8_t* AddressB = Address;
	for (size_t i = 0; i < RegionSize; i++)
	{
		AddressB[i] = i + i * i + i * i * i;
	}
	
	LogMsg("First UPtr: %p", *((uintptr_t*) AddressB));
	
	Status = OSFreeVirtualMemory(CURRENT_PROCESS_HANDLE, Address, RegionSize, MEM_RELEASE);
	CHECK_FAIL("release virtual memory");
}

void PerformMm2Test()
{
	DbgPrint("Waiting for logs to calm down.");
	PerformDelay(2000, NULL);
	
	// also wait a bit more for the init reclaimer to finish because it might affect our memory readings
	DbgPrint("Waiting for logs to calm down 2.");
	PerformDelay(200, NULL);
	
	// note: the first time you run this the PTEs might need to be allocated and so you "leak" 3 pages.
	// but run the test again and it will reuse those pages.
	//
	// in the future, I plan on adding a feature which deallocates empty PTs.
	
	DbgPrint("Mm2 test started.  Dumping VAD List and Heap.  There are %zu pages free.", MmGetTotalFreePages());
	MmDebugDumpVad();
	MmDebugDumpHeap();
	
	DbgPrint("Mm2 test doing the tests now.");
	Mm2BasicReserveTest();
	Mm2BasicCommitTest();
	Mm2TopDownMemoryTest();
	Mm2ExposedApiTest();
	
	DbgPrint("Mm2 test complete.  Dumping VAD List and Heap.  There are %zu pages free.", MmGetTotalFreePages());
	MmDebugDumpVad();
	MmDebugDumpHeap();
}
