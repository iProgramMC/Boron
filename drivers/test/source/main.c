/***
	The Boron Operating System
	Copyright (C) 2023-2024 iProgramInCpp

Module name:
	main.c
	
Abstract:
	This module implements the main function of the test
	driver.
	
Author:
	iProgramInCpp - 20 October 2023
***/
#include "utils.h"
#include "tests.h"

PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter)
{
	PKTHREAD Thread = KeAllocateThread();
	if (!Thread)
		return NULL;
	
	if (FAILED(KeInitializeThread(
		Thread,
		POOL_NO_MEMORY_HANDLE,
		StartRoutine,
		Parameter,
		KeGetSystemProcess())))
	{
		KeDeallocateThread(Thread);
		return NULL;
	}
	
	KeSetPriorityThread(Thread, PRIORITY_NORMAL);
	
	KeReadyThread(Thread);
	return Thread;
}

NO_RETURN void DriverTestThread(UNUSED void* Parameter)
{
	//LogMsg(ANSI_RED "Starting process test" ANSI_RESET);
	//PerformProcessTest();
	//LogMsg(ANSI_RED "Starting mutex test" ANSI_RESET);
	//PerformMutexTest();
	//PerformBallTest();
	//PerformFireworksTest();
	//LogMsg(ANSI_RED "Starting handle test" ANSI_RESET);
	//PerformHandleTest();
	LogMsg(ANSI_RED "Starting APC test" ANSI_RESET);
	PerformApcTest();
	//LogMsg(ANSI_RED "Starting rwlock test" ANSI_RESET);
	//PerformRwlockTest();
	//LogMsg(ANSI_RED "Starting object test" ANSI_RESET);
	//PerformObjectTest();
	//LogMsg(ANSI_RED "Starting MDL test" ANSI_RESET);
	//PerformMdlTest();
	//LogMsg(ANSI_RED "Starting interrupt test" ANSI_RESET);
	//PerformIntTest();
	
	LogMsg(ANSI_GREEN "*** All tests have concluded." ANSI_RESET);
	KeTerminateThread(0);
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	PKTHREAD Thread = CreateThread(DriverTestThread, NULL);
	
	if (!Thread)
		return STATUS_INSUFFICIENT_MEMORY;
	
	KeDetachThread(Thread, NULL);
	return STATUS_SUCCESS;
}
