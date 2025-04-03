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
#include <ps.h>

PKTHREAD CreateThread(PKTHREAD_START StartRoutine, void* Parameter)
{
	HANDLE Handle = HANDLE_NONE;
	BSTATUS Status = PsCreateSystemThread(&Handle, NULL, HANDLE_NONE, StartRoutine, Parameter);
	
	if (FAILED(Status))
		KeCrash("Failed to CreateThread(%p): status %d", StartRoutine, Status);
	
	void* Thrd = NULL;
	Status = ObReferenceObjectByHandle(Handle, NULL, &Thrd);
	if (FAILED(Status))
		KeCrash("Failed to reference handle while creating thread: status %d", Status);
	
	ObClose(Handle);
	return Thrd;
}

void PerformDelay(int Ms, PKDPC Dpc)
{
	KTIMER Timer;
	
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, Ms, Dpc);
	
	KeWaitForSingleObject(&Timer.Header, false, TIMEOUT_INFINITE, MODE_KERNEL);
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
	PerformKeyboardTest();
	//PerformStorageTest();
	//PerformExObTest();
	//PerformCcbTest();
	//PerformMm1Test();
	//PerformMm2Test();
	
	LogMsg(ANSI_GREEN "*** All tests have concluded." ANSI_RESET);
	KeTerminateThread(0);
}

BSTATUS DriverEntry(UNUSED PDRIVER_OBJECT Object)
{
	PKTHREAD Thread = CreateThread(DriverTestThread, NULL);
	
	if (!Thread)
		return STATUS_INSUFFICIENT_MEMORY;
	
	ObDereferenceObject(Thread);
	return STATUS_SUCCESS;
}
