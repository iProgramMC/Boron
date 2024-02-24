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

void ApcSpecialRoutine(PKAPC Apc)
{
	LogMsg("Hello from ApcSpecialRoutine!");
}

void ApcNormalRoutine(void* Context, void* SystemArgument1, void* SystemArgument2)
{
	LogMsg("Hello from ApcNormalRoutine!");
}

void PerformApcTest()
{
	KAPC Apc;
	LogMsg("Initializing APC...");
	KeInitializeApc(&Apc, KeGetCurrentThread(), ApcSpecialRoutine, ApcNormalRoutine, NULL, MODE_KERNEL);
	LogMsg("Enqueuing APC...");
	KeInsertQueueApc(&Apc, NULL, NULL);
	LogMsg("Done, should execute now.  Waiting 100 sec...");
	
	KTIMER Timer;
	KeInitializeTimer(&Timer);
	KeSetTimer(&Timer, 100000, NULL);
	KeWaitForSingleObject(&Timer, false, TIMEOUT_INFINITE);
}
