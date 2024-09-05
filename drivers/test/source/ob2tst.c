/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	ob2tst.c
	
Abstract:
	This module, part of the test driver, implements the
	executive dispatch object test.
	
Author:
	iProgramInCpp - 10 August 2024
***/
#include <ex.h>
#include <string.h>
#include "tests.h"
#include "utils.h"

// ******** Mutex Object Test ******** //
HANDLE gMutexHandle;

#define CheckError(Status) do {       \
	if ((Status) != STATUS_SUCCESS) { \
		KeCrash("error at %s:%d (%s), status %d", __FILE__, __LINE__, __func__, (Status)); \
	}                                 \
} while (0)

// Description:
// Thread 1 will lock the mutex for 1 second.
// Thread 2 will wait 200 ms and then acquire the mutex. Once acquired it will wait 1 second before releasing.
// Thread 3 will poll the mutex every 500ms or so to see if it's acquired or not (tests OSQueryMutex)

NO_RETURN void MtThread1Routine(UNUSED void* Context)
{
	LogMsg("Thr1 waiting on mutex");
	BSTATUS Status = OSWaitForSingleObject(gMutexHandle, false, TIMEOUT_INFINITE);
	CheckError(Status);
	
	LogMsg("Thr1 waiting 1s");
	PerformDelay(1000, NULL);
	
	LogMsg("Thr1 releasing mutex");
	Status = OSReleaseMutex(gMutexHandle);
	CheckError(Status);
	
	KeTerminateThread(1);
}

NO_RETURN void MtThread2Routine(UNUSED void* Context)
{
	LogMsg("Thr2 performing 200ms delay");
	
	PerformDelay(200, NULL);
	
	LogMsg("Thr2 waiting on mutex");
	BSTATUS Status = OSWaitForSingleObject(gMutexHandle, false, TIMEOUT_INFINITE);
	CheckError(Status);
	
	LogMsg("Thr2 waiting 1s");
	PerformDelay(1000, NULL);
	
	LogMsg("Thr2 releasing mutex");
	Status = OSReleaseMutex(gMutexHandle);
	CheckError(Status);
	
	KeTerminateThread(1);
}

NO_RETURN void MtThread3Routine(UNUSED void* Context)
{
	LogMsg("Thr3 is up");
	
	for (int i = 0; i < 10; i++)
	{
		int State = 0;
		BSTATUS Status = OSQueryMutex(gMutexHandle, &State);
		CheckError(Status);
		
		LogMsg("Thr3: State is %d", State);
		
		PerformDelay(500, NULL);
	}
	
	KeTerminateThread(1);
}

void PerformExObMutexTest()
{
	PKTHREAD Threads[3];
	
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName    = "\\Test Mutex";
	Attributes.RootDirectory = HANDLE_NONE;
	Attributes.ObjectNameLength = strlen(Attributes.ObjectName);
	
	BSTATUS Status = OSCreateMutex(&gMutexHandle, &Attributes);
	CheckError(Status);
	
	Threads[0] = CreateThread(MtThread1Routine, NULL);
	Threads[1] = CreateThread(MtThread2Routine, NULL);
	Threads[2] = CreateThread(MtThread3Routine, NULL);
	
	Status = KeWaitForMultipleObjects(3, (void**) Threads, WAIT_ALL_OBJECTS, false, TIMEOUT_INFINITE, NULL);
	CheckError(Status);
	
	OSClose(gMutexHandle);
	gMutexHandle = HANDLE_NONE;
	
	for (int i = 0; i < (int)ARRAY_COUNT(Threads); i++)
		ObDereferenceObject(Threads[i]);
	
	LogMsg(ANSI_GREEN ">> ExObMutexTest done." ANSI_RESET);
}

// ******** Event Object Test ******** //

HANDLE gEventHandle;

NO_RETURN void EtThread1Routine(UNUSED void* Context)
{
	BSTATUS Status;
	
	LogMsg("Thr1: waiting on the event");
	Status = OSWaitForSingleObject(gEventHandle, false, TIMEOUT_INFINITE);
	CheckError(Status);
	
	LogMsg("Thr1: done waiting on the event, waiting 1s");
	PerformDelay(1000, NULL);
	
	LogMsg("Thr1: setting event, thr0 should wake up now");
	OSSetEvent(gEventHandle);
	
	LogMsg("Thr1: Done");
	
	KeTerminateThread(1);
}

void PerformExObEventTest()
{
	OBJECT_ATTRIBUTES Attributes;
	Attributes.ObjectName    = "\\Test Event";
	Attributes.RootDirectory = HANDLE_NONE;
	Attributes.ObjectNameLength = strlen(Attributes.ObjectName);
	
	BSTATUS Status = OSCreateEvent(&gEventHandle, &Attributes, EVENT_SYNCHRONIZATION, false);
	CheckError(Status);
	
	PKTHREAD Thread = CreateThread(EtThread1Routine, NULL);
	
	// Wait a second, then set the event.
	LogMsg("Thr0: Event created, waiting a sec and setting it");
	PerformDelay(1000, NULL);
	
	LogMsg("Thr0: Setting event and waiting 100ms");
	Status = OSSetEvent(gEventHandle);
	CheckError(Status);
	PerformDelay(100, NULL);
	
	LogMsg("Thr0: Waiting on the event");
	Status = OSWaitForSingleObject(gEventHandle, false, TIMEOUT_INFINITE);
	CheckError(Status);
	
	LogMsg("Thr0: Done");
	
	KeWaitForSingleObject(Thread, false, TIMEOUT_INFINITE);
	
	ObDereferenceObject(Thread);
	
	LogMsg(ANSI_GREEN ">> ExObEventTest done." ANSI_RESET);
}

void PerformExObTest()
{
	PerformExObEventTest();
	PerformExObMutexTest();
}
