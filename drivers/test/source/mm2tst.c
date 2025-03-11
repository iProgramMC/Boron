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
#include "utils.h"

// Note : These aren't the actual APIs userspace sees.

void Mm2BasicReserveTest()
{
	// Test a basic case, where you just reserve one range of memory and then free it.
	void* Address = NULL;
	BSTATUS Status;
	
	Status = MmReserveVirtualMemory(400, &Address, MEM_RESERVE, 0);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to reserve virtual memory with status %d", __LINE__, Status);
	
	// Let's reserve another!
	void* Address2 = NULL;
	Status = MmReserveVirtualMemory(40000, &Address2, MEM_RESERVE, 0);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to reserve virtual memory with status %d", __LINE__, Status);
	
	LogMsg("Memory reserved at %p(#1) and %p(#2)", Address, Address2);
	
	// Free the first one and try to reserve a bigger one in its place.
	Status = MmReleaseVirtualMemory(Address);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to release virtual memory with status %d", __LINE__, Status);
	
	Status = MmReserveVirtualMemory(400000, &Address, MEM_RESERVE, 0);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to reserve virtual memory with status %d", __LINE__, Status);
	
	LogMsg("New #1 address: %p", Address);
	
	// Reallocate the second one with the same size.
	Status = MmReleaseVirtualMemory(Address2);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to release virtual memory with status %d", __LINE__, Status);
	
	Status = MmReserveVirtualMemory(40000, &Address2, MEM_RESERVE, 0);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to reserve virtual memory with status %d", __LINE__, Status);
	
	LogMsg("New #2 address: %p", Address2);
	
	MmDebugDumpVad();
	
	// The end
	Status = MmReleaseVirtualMemory(Address);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to release virtual memory with status %d", __LINE__, Status);
	
	Status = MmReleaseVirtualMemory(Address2);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to release virtual memory with status %d", __LINE__, Status);
}

void Mm2BasicCommitTest()
{
	const size_t PageCount = 400;
	
	// Reserve some space, then commit it, test out the new committed region, decommit and release.
	void* Address = NULL;
	BSTATUS Status;
	
	Status = MmReserveVirtualMemory(PageCount, &Address, MEM_RESERVE, PAGE_READ | PAGE_WRITE);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to reserve virtual memory with status %d", __LINE__, Status);
	
	LogMsg("Reserved at %p", Address);
	
	Status = MmCommitVirtualMemory((uintptr_t) Address, PageCount);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to commit virtual memory with status %d", __LINE__, Status);
	
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
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to decommit virtual memory with status %d", __LINE__, Status);
	
	Status = MmReleaseVirtualMemory(Address);
	if (FAILED(Status))
		KeCrash("MM2[%d]: Failed to release virtual memory with status %d", __LINE__, Status);
}

void Mm2AnotherCommitTest()
{
	
}

void PerformMm2Test()
{
	Mm2BasicReserveTest();
	Mm2BasicCommitTest();
	Mm2AnotherCommitTest();
	
	DbgPrint("Dumping vad");
	MmDebugDumpVad();
}
