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
		NULL,
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

void PerformDelay(int Ms, PKDPC Dpc)
{
	KTIMER Timer;
	
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, Ms, Dpc);
	
	KeWaitForSingleObject(&Timer.Header, false, TIMEOUT_INFINITE);
}

NO_RETURN void DriverTestThread(UNUSED void* Parameter)
{
	//PerformProcessTest();
	//PerformMutexTest();
	//PerformBallTest();
	//PerformFireworksTest();
	//PerformHandleTest();
	//PerformApcTest();
	//PerformRwlockTest();
	//PerformObjectTest();
	//PerformMdlTest();
	//PerformIntTest();
	//PerformKeyboardTest();
	//PerformStorageTest();
	PerformExObTest();
	//PerformCcbTest();
	//PerformMm1Test();
	
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
