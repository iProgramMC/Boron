/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	proctst.c
	
Abstract:
	This module implements the process test for the test driver.
	It tests processes, synchronization events, and address probing.
	
Author:
	iProgramInCpp - 25 November 2023
***/
#include "utils.h"
#include "tests.h"

uint64_t* const TheMemory = (uint64_t*) 0x90000000000;
const size_t SizeOfTheMemory = 0x100000;

KPROCESS Proc;

KTHREAD Thrd;

KEVENT Evnt;

NO_RETURN
void ProcessTestRoutine(UNUSED void* Ptr)
{
	// Map in the memory.
	
	// TODO: locking?
	
	HPAGEMAP Map = MmGetCurrentPageMap();
	
	MmMapAnonPages(Map, (uintptr_t) TheMemory, SizeOfTheMemory / PAGE_SIZE, MM_PTE_READWRITE | MM_PTE_SUPERVISOR, true);
	
	// Probe the memory.
	int Status = MmProbeAddress(TheMemory, SizeOfTheMemory, true, false, NULL);
	
	LogMsg("Status In Process: %d (before signalling event)", Status);
	
	// Signal the event.
	KePulseEvent(&Evnt);
	
	// Probe the memory again.
	Status = MmProbeAddress(TheMemory, SizeOfTheMemory, true, false, NULL);
	LogMsg("Status In Process: %d (after signalling event)", Status);
	
	KeTerminateThread();
}

void PerformProcessTest()
{
	KeInitializeProcess(
		&Proc,
		PRIORITY_NORMAL,
		AFFINITY_ALL
	);
	
	// Process initialized, spin up a thread.
	KeInitializeThread(
		&Thrd,
		EX_NO_MEMORY_HANDLE,
		ProcessTestRoutine,
		NULL,
		&Proc
	);
	
	// Initialize an unsignaled event.
	KeInitializeEvent(
		&Evnt,
		EVENT_SYNCHRONIZATION,
		false
	);
	
	// Ready the thread.
	KeReadyThread(&Thrd);
	
	// Wait for the event to be pulsed.
	KeWaitForSingleObject(
		&Evnt,
		false,
		TIMEOUT_INFINITE
	);
	
	// Probe the memory.
	int Status = MmProbeAddress(TheMemory, SizeOfTheMemory, true, false, NULL);
	
	// Should be non zero.
	LogMsg("Status In System : %d", Status);
}
