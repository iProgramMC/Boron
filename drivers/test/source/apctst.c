/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	apctst.c
	
Abstract:
	This module implements the APC test for the test driver.
	
Author:
	iProgramInCpp - 19 November 2023
***/
#include "utils.h"
#include <ke.h>

static void ApcSpecialRoutine(UNUSED PKAPC Apc, UNUSED PKAPC_NORMAL_ROUTINE* R, UNUSED void** A, UNUSED void** B, UNUSED void** C)
{
	LogMsg("Hello from ApcSpecialRoutine!  I'm coming from %p", KeGetCurrentThread());
}

static void ApcNormalRoutine(UNUSED void* Context, UNUSED void* SystemArgument1, UNUSED void* SystemArgument2)
{
	LogMsg("Hello from ApcNormalRoutine!  I'm coming from %p", KeGetCurrentThread());
}

static NO_RETURN void ApcTestRoutine()
{
	LogMsg("Hello from ApcTestRoutine! My thread ptr is %p", KeGetCurrentThread());
	
	while (true) {
		ASM("hlt");
	}
}

static void PerformDelay(int Ms)
{
	KTIMER Timer;
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, Ms, NULL);
	KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
}

void PerformApcTest()
{
	KTHREAD Thread;
	ASSERT(KeInitializeThread(&Thread, POOL_NO_MEMORY_HANDLE, ApcTestRoutine, NULL, KeGetCurrentProcess()) == STATUS_SUCCESS);
	KeReadyThread(&Thread);
	
	// wait a bit.
	PerformDelay(1000);
	
	KAPC Apc;
	LogMsg("Initializing APC...");
	KeInitializeApc(&Apc, &Thread, ApcSpecialRoutine, ApcNormalRoutine, NULL, MODE_KERNEL);
	LogMsg("Enqueuing APC...");
	KeInsertQueueApc(&Apc, NULL, NULL);
	
	LogMsg("Done, should execute now.  Waiting 100 sec...");
	PerformDelay(100000);
}
