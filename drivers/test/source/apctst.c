/***
	The Boron Operating System
	Copyright (C) 2024 iProgramInCpp

Module name:
	apctst.c
	
Abstract:
	This module implements the APC test for the test driver.
	
Author:
	iProgramInCpp - 24 February 2024
***/
#include "utils.h"
#include <ke.h>

static KEVENT Event;

static void ApcSpecialRoutine(UNUSED PKAPC Apc, UNUSED PKAPC_NORMAL_ROUTINE* R, UNUSED void** A, UNUSED void** B, UNUSED void** C)
{
	LogMsg("Hello from ApcSpecialRoutine!  I'm coming from %p", KeGetCurrentThread());
}

static void ApcNormalRoutine(UNUSED void* Context, UNUSED void* SystemArgument1, UNUSED void* SystemArgument2)
{
	LogMsg("Hello from ApcNormalRoutine!  I'm coming from %p", KeGetCurrentThread());
	
	KeSetEvent(&Event, 1);
}

static NO_RETURN void ApcTestRoutine()
{
	LogMsg("Hello from ApcTestRoutine! My thread ptr is %p", KeGetCurrentThread());
	
	KeInitializeEvent(&Event, EVENT_SYNCHRONIZATION, false);
	
	KeWaitForSingleObject(&Event, false, TIMEOUT_INFINITE);
	
	LogMsg("Exiting");
	
	KeTerminateThread(1);
	
	/*
	while (true) {
		ASM("hlt");
	}*/
}

void PerformApcTest()
{
	KTHREAD Thread;
	ASSERT(KeInitializeThread(&Thread, POOL_NO_MEMORY_HANDLE, ApcTestRoutine, NULL, KeGetCurrentProcess()) == STATUS_SUCCESS);
	KeReadyThread(&Thread);
	
	// wait a bit.
	PerformDelay(1000, NULL);
	
	KAPC Apc;
	LogMsg("Initializing APC...");
	KeInitializeApc(&Apc, &Thread, ApcSpecialRoutine, ApcNormalRoutine, NULL, MODE_KERNEL);
	LogMsg("Enqueuing APC...");
	KeInsertQueueApc(&Apc, NULL, NULL, 0);
	
	LogMsg("Done, should execute now.  Waiting for the thread to exit...");
	
	KeWaitForSingleObject(&Thread, false, TIMEOUT_INFINITE);
}
