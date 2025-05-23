/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	mutextst.c
	
Abstract:
	This module implements the mutex test for the test driver.
	
Author:
	iProgramInCpp - 19 November 2023
***/
#include "utils.h"

// Convert an integer to a void* to pass in to the start routine.
// Won't be used as an actual pointer.
#define P(n) ((void*) (uintptr_t) (n))

// Global mutex used in the test.
static KMUTEX TestMutex;
static KMUTEX TestMutex2;

#define DELAY (100)

NO_RETURN static void MutexTestThread(UNUSED void* Parameter)
{
	int ThreadNum = (int)(uintptr_t)Parameter;
	
	DbgPrint("Hello from MutexTestThread #%d", ThreadNum);
	
	for (int i = 0; i < 10; i++)
	{
		LogMsg("Thread %d Acquiring Mutexes...", ThreadNum);
		
		// Acquire the mutexes.
		void* Objects[] = { &TestMutex, &TestMutex2 };
		KeWaitForMultipleObjects(2, Objects, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, NULL, MODE_KERNEL);
		
		LogMsg("Thread %d Waiting %d ms...", ThreadNum, DELAY);
		// Wait a second.
		KTIMER Timer;
		KeInitializeTimer(&Timer);
		KeSetTimer(&Timer, DELAY, NULL);
		KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE, MODE_KERNEL);
		
		// Finally, release the mutexes.
		LogMsg("Thread %d Releasing Mutexes...", ThreadNum);
		KeReleaseMutex(&TestMutex);
		KeReleaseMutex(&TestMutex2);
	}
	
	KeTerminateThread(0);
}

void PerformMutexTest()
{
	KeInitializeMutex(&TestMutex, 1);
	KeInitializeMutex(&TestMutex2, 1);
	
	PKTHREAD Thrd1 = CreateThread(MutexTestThread, P(1));
	PKTHREAD Thrd2 = CreateThread(MutexTestThread, P(2));
	
	void* Objects[] =
	{
		Thrd1,
		Thrd2
	};
	
	// Wait for all threads to exit.
	KeWaitForMultipleObjects(2, Objects, WAIT_TYPE_ALL, false, TIMEOUT_INFINITE, NULL, MODE_KERNEL);
	
	// Get rid of the threads.
	ObDereferenceObject(Thrd1);
	ObDereferenceObject(Thrd2);
	
	LogMsg("*** All threads have exited.");
}

