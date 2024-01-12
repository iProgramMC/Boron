/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	proctst.c
	
Abstract:
	This module implements the rwlock test for the test driver.
	It tests the functionality of the rwlock.
	
Author:
	iProgramInCpp - 11 January 2024
***/
#include <ex.h>
#include "utils.h"
#include "tests.h"

// Convert an integer to a void* to pass in to the start routine.
// Won't be used as an actual pointer.
#define P(n) ((void*) (uintptr_t) (n))

EX_RW_LOCK RWLock;
int SomeVariable;

NO_RETURN
void ExclusiveRoutine(UNUSED void* Ptr)
{
	while (true)
	{
		//DbgPrint("Exclusively Acquiring Lock...");
		ExAcquireExclusiveRwLock(&RWLock, false, false);
		//DbgPrint("Exclusively Acquired Lock...");
		
		SomeVariable++;
		LogMsg("ExclusiveRoutine: SomeVariable is now %d, waiting a sec.", SomeVariable);
		
		// wait a sec
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, 1000, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
		
		//DbgPrint("Exclusively Releasing Lock...");
		ExReleaseRwLock(&RWLock);
		//DbgPrint("Exclusively Released Lock...");
	}
}

NO_RETURN
void ReaderRoutine(void* Ptr)
{
	int Something = (int)(uintptr_t)Ptr;
	
	while (true)
	{
		//DbgPrint("Thread %d Acquiring Lock...", Something);
		ExAcquireSharedRwLock(&RWLock, false, false, false);
		
		//DbgPrint("Thread %d Acquired Lock...", Something);
		LogMsg("Thread %d: Value is %d", Something, SomeVariable);
		
		//DbgPrint("Thread %d Releasing Lock...", Something);
		ExReleaseRwLock(&RWLock);
		//DbgPrint("Thread %d Released Lock...", Something);
		
		// wait a sec
		//KTIMER Timer;
		//KeInitializeTimer(&Timer);
		//KeSetTimer(&Timer, 200, NULL);
		//KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
	}
}

void PerformRwlockTest()
{
	//KSEMAPHORE Sem;
	//KeInitializeSemaphore(&Sem, 10, 0);
	//
	//for (int i = 0; i < 15; i++) {
	//	LogMsg("Waiting %d", i);
	//	KeWaitForSingleObject(&Sem, false, TIMEOUT_INFINITE);
	//	KeReleaseSemaphore(&Sem, 1);
	//}
	
	ExInitializeRwLock(&RWLock);
	
	PKTHREAD Rdr1 = CreateThread(ReaderRoutine, P(1));
	PKTHREAD Rdr2 = CreateThread(ReaderRoutine, P(2));
	PKTHREAD Rdr3 = CreateThread(ReaderRoutine, P(3));
	PKTHREAD Rdr4 = CreateThread(ReaderRoutine, P(4));
	PKTHREAD Excl = CreateThread(ExclusiveRoutine, NULL);
	
	DbgPrint("Excl: %p", Excl);
	DbgPrint("Rdr1: %p", Rdr1);
	DbgPrint("Rdr2: %p", Rdr2);
	DbgPrint("Rdr3: %p", Rdr3);
	DbgPrint("Rdr4: %p", Rdr4);
	
	ASSERT(Excl && Rdr1 && Rdr2 && Rdr3 && Rdr4);
	
	KWAIT_BLOCK WaitBlockStorage[MAXIMUM_WAIT_BLOCKS];
	void* Objects[] = { Excl, Rdr1, Rdr2, Rdr3, Rdr4 };
	KeWaitForMultipleObjects(5, Objects, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, WaitBlockStorage);
	
	// get rid of the threads
	KeDeallocateThread(Excl);
	KeDeallocateThread(Rdr1);
	KeDeallocateThread(Rdr2);
	KeDeallocateThread(Rdr3);
	KeDeallocateThread(Rdr4);
}
